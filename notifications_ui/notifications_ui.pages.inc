<?php
// $Id: notifications_ui.pages.inc,v 1.1.2.3.6.8 2010/05/18 16:32:57 jareyero Exp $
/**
 * @file
 *   User pages for User Interface for subscriptions modules
 */
 
/**
 * User add subscription
 */
function notifications_ui_page_user_add($account, $type = NULL) {
  drupal_set_title(t('Add subscription'));
  if ($type && ($subscription = notifications_subscription($type))) {    
    $subscription = $subscription->get_instance()->set_user($account);
    return drupal_get_form('notifications_subscription_add_form', $subscription);
  }
  else {
    $options = array();
    foreach (notifications_subscription_user_types($account) as $type => $subscription) {
        $options[$type] = array(
          '#type' => 'link',
          '#markup' => $subscription->get_subscribe_text(),
          '#href'  => "user/$account->uid/notifications/add/$key",
          '#description' => $type->get_description(),
        );
      }
    }
    if ($options) {
      $build['title']['#markup'] = t('Select a susbscription type:');
      $build['options'] = $options;
    }
    else {
      $build['message'] = t('No available subscription types.');
    }
    return $build;
  }  
}

