<?php
// $Id: notifications.pages.inc,v 1.1.2.8.2.9.2.25 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * User pages for Notifications
 */

/**
 * Menu callback. Overview page for user subscriptions.
 * 
 * We create the page as a form so it can be altered
 * 
 */
function notifications_user_overview($form_state, $account) {
  $form['account'] = array('#type' => 'value', '#value' => $account);
  
  $path = 'user/' . $account->uid;
  
  // Status, send method, interval
  $count = db_query("SELECT status, COUNT(*) AS num FROM {notifications_subscription} WHERE uid = %d GROUP BY status", $account->uid);
  $status_list = Notifications_Subscription::status_list();
  while ($current = db_fetch_object($count)) {
    $status[$current->status] = t('You have %count @status subscriptions.', array('%count' => $current->num, '@status' => $status_list[$current->status]));
  }
  if (empty($status)) {
    $status['none'] = t('You don\'t have any subscriptions yet.');
  }
  $send_intervals = notifications_send_intervals();
  $send_methods = _notifications_send_methods($account);
  if ($method = notifications_user_setting('send_method', $account)) {
    if (isset($send_methods[$method])) {      
      $status['method'] = t('Your default sending method for new subscriptions is @send_method', array('@send_method' => $send_methods[$method]));
    }
  }
  $interval = notifications_user_setting('send_interval', $account);
  if (isset($send_intervals[$interval])) {      
    $status['interval'] = t('Your default sending interval for new subscriptions is @send_interval', array('@send_interval' => $send_intervals[$interval]));
  }
  
  $form['status'] = array('#type' => 'item', '#weight' => 10, '#title' => t('Current status'), '#value' => theme('item_list', $status));
  
  // Build shortcut tips
  if (notifications_access_user($account, 'manage')) {
    $tips['admin'] = l(t('Administer your subscriptions'), "$path/notifications/subscriptions");
  }
  $tips['edit'] = l(t('Edit your notifications settings'), "$path/edit");
  
  // Enable / disable all subscriptions
  if (notifications_access_user($account, 'maintain')) {
    if (!empty($status[Notifications_Subscription::STATUS_ACTIVE])) {
      $tips['disable'] = l(t('Temporarily disable all your subscriptions'), "$path/notifications/update/disable");
    }
    if (!empty($status[Notifications_Subscription::STATUS_INACTIVE]) || !empty($status[Notifications_Subscription::STATUS_BLOCKED])) {
      $tips['enable'] = l(t('Enable all your subscriptions'), "$path/notifications/update/enable");
    }    
  }
  $link = notifications_get_link('unsubscribe', array('uid' => $account->uid, 'destination' => TRUE));
  $tips['cancel'] = l(t('Cancel all your subscriptions'), $link['href'], $link['options']);
  
  //$output .= theme('item_list', $tips, t('You can'));
  $form['tips'] = array('#type' => 'item', '#weight' => 20, '#title' => t('You can'), '#value' => theme('item_list', $tips));
   
  return $form;  
}

/**
 * Menu callback: user subscriptions management
 */
function notifications_user_manage_subscriptions($form_state, $account) {
  module_load_include('inc', 'notifications', 'notifications.pages');
  if (isset($form_state['values']['operation']) && $form_state['values']['operation'] == 'delete') {
    $form = notifications_multiple_delete_confirm($form_state, array_filter($form_state['values']['subscriptions']));
    return $form;
  }
  $form = notifications_subscriptions_filter_form();
  $form['filters']['#title'] = t('Filter');
  // Display collapsed only when no filters set
  $form['filters'] += array('#collapsible' => TRUE, '#collapsed' => empty($_SESSION['subscriptions_overview_filter']));
  $form['#theme'] = 'notifications_subscriptions_filter_form';
  $form['admin']  = notifications_manage_subscriptions_form($account);

  return $form;
}

/**
 * List form for subscriptions of the same type for a user
 * 
 * @todo Add enable /disable operations
 * @todo Add pager
 * 
 * @param $type
 *   Subscription type
 * @param $account
 *   User account, owner of these subscriptions
 * @param $subscriptions
 *   Predefined array of subscriptions to display, so we don't search more nor allow new ones.
 * @param $fields
 *   Predefined field values for new subscriptions, as type => value array
 */
function notifications_subscription_list_form($form_state, $subscription_type, $account, $subscriptions = NULL, $fields = array()) {
  if (isset($form_state['values']['op']) && $form_state['values']['op'] == t('Delete')) {
    if ($selected = array_filter($form_state['values']['subscriptions']['select'])) {
      $form = notifications_multiple_delete_confirm($form_state, $selected);
      return $form;      
    }
  }
  notifications_include('object.inc');
  $account = messaging_user_object($account);
  $form['account'] = array('#type' => 'value', '#value' => $account);

  // Build a template subscription. These subscriptions should have a unique subscription variable field
  $template = notifications_build_subscription($subscription_type);
  $template->set_account($account);
  $template->set_fields($fields);
  $form['template'] = array('#type' => 'value', '#value' => $template); 
  // Print template information: subscription type, description, fixed fields
  //$form['info'] = $template->form_info();
  $form['info'] = array(
    '#type' => 'item',
    '#title' => t('@type subscriptions', array('@type' => $template->get_title())),
    '#description' => $template->get_description(),
  );

  // Just allow one field of each type to be editable
  $editable_fields = array();
  foreach ($template->get_editable_fields() as $field) {
    $editable_fields[$field->field] = $field;
  }
  $form['editable_fields'] = array('#type' => 'value', '#value' => $editable_fields);

  if (!isset($subscriptions)) {
    // We get all subscriptions that at least have the given fields, but may have more
    $subscriptions = notifications_get_subscriptions(array('type' => $subscription_type, 'uid' => $account->uid), $fields, FALSE);
  }
  // If we don't have subscriptions and we cannot create new ones, just display a message
  if (!$subscriptions) {
    $form['warning']['#value'] = t("There are no subscriptions of this type.");
  }
  $form['current_subscriptions'] = array('#type' => 'value', '#value' => $subscriptions);
  $subscription_list = $subscriptions; 
  // Add our template to the list, but not to the values in form
  if ($editable_fields && notifications_access_user_add($account, $subscription_type)) {
    $subscription_list['new'] = $template;
  }

  // Prepare other parameters to build the list
  $send_methods = notifications_send_methods($account);
  $send_intervals = notifications_send_intervals($account);

  // Build header with selector, fields, send interval, send method if apply
  $header = array(theme('table_select_header_cell'));
  $table_fields = array('select');
  foreach ($editable_fields as $type => $field) {
    $header[] = notifications_field_format_name($field->field);
    $table_fields[] = 'field_' . $type;
  }
  // Hide Send method, send interval columns if only one
  if (count($send_intervals)) {
    $header[] = t('Send interval');
    $table_fields[] = 'send_interval';
  }
  else {
    $send_intervals = NULL;
  }
  if (count($send_methods) > 1) {
    $header[] = t('Send method');
    $table_fields[] = 'send_method';
  }
  else {
    $send_methods = NULL;
  }

  $form['subscriptions'] = array(
    '#type' => 'fieldset',
    '#tree' => TRUE,
    '#theme' => 'notifications_table_form',
    '#table_header' => $header,
    '#table_fields' => $table_fields,
  );
  // Add our template to the subscription list and
  $elements = array();
  foreach ($subscription_list as $key => $subscription) {
    if ($subscription->is_instance()) {
      $form['subscriptions']['select'][$key] = array(
        '#type' => 'checkbox',
        '#default_value' => FALSE, // All unselected by default
      );
    }
    else {
      $form['subscriptions']['select']['new'] = array(
        '#type' => 'submit',
        '#value' => t('Add'),
      );
    }
    foreach ($editable_fields as $type => $field) {
      if ($subscription->is_instance()) {
        $conditions = $subscription->get_conditions();
        $value = $conditions[$type];
        $form['subscriptions']['field_' . $type][$key] = array('#value' => notifications_field_format_value($type, $value));
      }
      else {
        // Add new subscription, editable fields
        $form['subscriptions']['field_' . $type][$key] = notifications_field_form_element($type, NULL, $subscription, FALSE, FALSE, 30);
      }
    }
    if ($send_intervals)
      $form['subscriptions']['send_interval'][$key] = array(
      '#type' => 'select',
      '#options' => $send_intervals,
      '#default_value' => $subscription->send_interval,
    );
    if ($send_methods) {
      $form['subscriptions']['send_method'][$key] = array(
        '#type' => 'select',
        '#options' => $send_methods,
        '#default_value' => $subscription->send_method,
      );
    }
  }
  if ($subscriptions) {
    $form['operations'] = array(
      '#type' => 'fieldset',
    );
    $form['operations']['update'] = array('#type' => 'submit', '#value' => t('Update'));
    $form['operations']['delete'] = array('#type' => 'submit', '#value' => t('Delete'));
    //$form['operations']['enable'] = array('#type' => 'submit', '#value' => t('Enable'));
    //$form['operations']['disable'] = array('#type' => 'submit', '#value' => t('Disable'));
  }
  return $form;
}

/**
 * Submit list of subscriptions
 */
function notifications_subscription_list_form_validate($form, &$form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : NULL;
  switch ($op) {
    case t('Add'):
      $save_fields = array();
      foreach ($form_state['values']['editable_fields'] as $field) {
        $type = $field->field;
        $key = 'field_' . $type;
        $value = isset($form_state['values']['subscriptions'][$key]['new']) ? $form_state['values']['subscriptions'][$key]['new'] : NULL;
        if ($real_value = notifications_field_real_value($type, $value)) {
          $save_fields[] = array('type' => $type, 'value' => $real_value);
        }
        else {
          form_set_error("subscriptions][$key][new", t('You must set a valid value for this field.'));
        }
      }
      $form_state['values']['parsed_fields'] = $save_fields;
      break;
    case t('Delete'):
    case t('Enable'):
    case t('Disable'):
      // For this cases check only that we have selected some subscriptions
      $selected = array_filter($form_state['values']['subscriptions']['select']);
      if (isset($selected['new'])) {
        unset($selected['new']);
        unset($form_state['values']['subscriptions']['select']['new']);
      }
      if (!$selected) { //count($selected) < 1) {
        form_set_error('', t('You must select some subscriptions for that operation.'));
      }
      break;
  }
}

/**
 * Submit list of subscriptions
 */
function notifications_subscription_list_form_submit($form, &$form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : NULL;
  $updated = 0;
  switch ($op) {
    case t('Add'):
      $subscription = notifications_build_subscription($form_state['values']['template']);
      $subscription->add_fields($form_state['values']['parsed_fields']);
      notifications_subscription_list_form_values($form_state['values']['subscriptions'], $subscription, 'new');
      notifications_submit_subscription($subscription);
      break;
    case t('Update');
      $updated = 0;
      foreach ($form_state['values']['current_subscriptions'] as $key => $subscription) {
        if (notifications_subscription_list_form_values($form_state['values']['subscriptions'], $subscription)) {
          $subscription->save();
          $updated++;
        }
      }
      break;
    case t('Delete');
      $form_state['rebuild'] = TRUE;
      break;

  }
  if ($updated) {
    drupal_set_message(format_plural($updated, 'One subscription has been updated.', '@count subscriptions have been updated.'));
  }
}

/**
 * Extract values from subscription list form
 */
function notifications_subscription_list_form_values($form_values, $subscription, $key = NULL) {
  $key = isset($key) ? $key : $subscription->sid;
  $values = array();
  foreach (array('send_method', 'send_interval', 'status') as $field) {
    if (isset($form_values[$field][$key]) && (!isset($subscription->$field) || $subscription->$field != $form_values[$field][$key])) {
      // We just set the values that have changed and return them if any
      $subscription->$field = $values[$field] = $form_values[$field][$key];
    }         
  }
  return $values;
}

/**
 * Bulk update all user subscriptions, confirmation form
 * 
 * @param $account
 *   User account
 * @param $operation
 *   Operation to perform: enable, disable
 */
function notifications_user_update_subscriptions($form_state, $account, $operation) {
  $form['account'] = array('#type' => 'value', '#value' => $account);
  $form['operation'] = array('#type' => 'value', '#value' => $operation);  
  $destination = 'user/' . $account->uid . '/notifications';
  $form['#redirect'] = $destination;
  if ($operation == 'enable') {
    return confirm_form($form,
                      t('Are you sure you want to enable all your subscriptions?'),
                      $destination, t('You will get notifications again for all of them.'),
                      t('Enable'), t('Cancel'));
  }
  elseif ($operation == 'disable') {
    return confirm_form($form,
                      t('Are you sure you want to disable all your subscriptions?'),
                      $destination, t('You will stop getting notifications until you enable them again.'),
                      t('Disable'), t('Cancel'));    
  }
  else {
    drupal_access_denied();
  }
}

/**
 * Mass update user subscriptions
 */
function notifications_user_update_subscriptions_submit($form, $form_state) {
  $account = $form_state['values']['account'];
  
  switch ($form_state['values']['operation']) {
    case 'enable':
      // Enable also blocked subscriptions just in case there are some
      db_query('UPDATE {notifications_subscription} SET status = %d WHERE uid = %d', Notifications_Subscription::STATUS_ACTIVE, $account->uid);
      drupal_set_message(t('All your subscriptions have been enabled.'));
      break;
    case 'disable':
      db_query('UPDATE {notifications_subscription} SET status = %d WHERE status = %d AND uid = %d', Notifications_Subscription::STATUS_INACTIVE, Notifications_Subscription::STATUS_ACTIVE, $account->uid);
      notifications_queue()->queue_clean(array('uid' => $account->uid));
      drupal_set_message(t('All your subscriptions have been disabled.'));
      break;
  }
}

/**
 * Form for unsubscription confirmation
 * 
 * It works for both single subscription or account (all subscriptions)
 */
function notifications_user_form_unsubscribe_confirm($form_state, $subscription, $account = NULL) {
  // Pass on subscription values
  $form['subscription'] = array('#type' => 'value', '#value' => $subscription);
  $form['account'] = array('#type' => 'value', '#value' => $account);
  if ($subscription) {
    $message = t('Are you sure you want to delete this subscription?');
    // The subscription description will be added here
    $form['info'] = $subscription->form_info();    
  }
  else {
    $message = t('Are you sure you want to remove all your subscriptions on this site?');
  }

  return confirm_form($form,
    $message,
    isset($_GET['destination']) ? $_GET['destination'] : '',
    t('This action cannot be undone.'),
    t('Unsubscribe'),
    t('Cancel')
  );
}

/**
 * Process unsubscribe form submission
 */
function notifications_user_form_unsubscribe_confirm_submit($form, &$form_state) {
  $subscription = $form_state['values']['subscription'];
  $account = $form_state['values']['account'];
  switch ($form_state['values']['op']) {
    case t('Unsubscribe'):
      if ($subscription) {
        Notifications_Subscription::delete_subscription($subscription->sid);
        drupal_set_message(t('Your subscription has been removed.'));
      }
      elseif ($account) {
        Notifications_Subscription::delete_multiple(array('uid' => $account->uid));
        drupal_set_message(t('All your subscriptions have been removed.'));
      }
      $form_state['redirect'] = '';
      break;
    case t('Cancel'):
      // Do nothing, not worth showing a message, just get back
      $form_state['redirect'] = '';
      break;
  }
}

