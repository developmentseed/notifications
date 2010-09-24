<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Base class for all Notifications objects
 */
abstract class Notifications_Object {
  /**
   * Constructor
   */
  public function __construct($template = NULL) {
    if ($template) {
      $properties = (array)$template;
      foreach ($properties as $key => $value) {
        $this->$key = $value;
      }
    }    
  }
  /**
   * They need a build function
   */
  public static abstract function build_object($object);
}

/**
 * Wrapper for Drupal objects
 */
class Notifications_Drupal_Object {
  public $type;
  public $key;
  public $name;
  protected $object;
  protected $fields;
  /**
   * Constructor
   */
  public function __construct($value) {
    if (is_numeric($value) || is_string($value)) {
      $this->key = $value;
    }
    else {
      $this->object = $value;
      $this->key = $this->key();
    }
  }
  /**
   * Build from type, value
   */
  public static function build($type, $value) {
    if ($class = notifications_object_type($type, 'class')) {
      return new $class($value);
    }
    else {
      $object = new Notifications_Object($value);
      $object->type = $type;
      return $object;
    }
  }
  // Access callback
  function user_access($account) {
    return TRUE;
  }
  /**
   * Get object key
   */
  public function key() {
    return NULL;
  }
  /**
   * Get object value
   */
  public function value() {
    if (!isset($this->object)) {
      $object = $this->load();
      $this->object = $object ? $object : FALSE
    }
    return $this->object ? $this->object : NULL;
  }
  /**
   * Load related object or data
   */
  public function load() {
    return NULL;
  }  
  /**
   * Check user access
   */
  function user_access($account) {
    return (bool)$this->value();
  }
  /**
   * Get fields for this object type
   */
  function get_fields() {
    if (!isset($this->fields)) {
      $this->fields = array();
      if ($object = $this->value()) {
        // As this does an array_merge_recursive() we get grouped field => array(value1, value2..)
        $fields = module_invoke_all('notifications_object_' .$this->type, 'fields', $object);
        // Now we just need to filter out duplicate values
        foreach ($fields as $field) {
          $this->fields[$field->key()] = $field;
        }
      }
    }
    return $this->fields;
  }
  /**
   * Get object name, unfiltered string
   */
  function name() {
    return isset($this->name) ? $this->name : '';
  }
  /**
   * Get unique index for this object
   */
  function index() {
    return isset($this->key) ? $this->type . ':' . $this->key : $this->type;
  }
}

/**
 * Node objects
 */
class Notifications_Node extends Notifications_Drupal_Object {
  public $type = 'node';
  /**
   * Load related object or data
   */
  public function load() {  
    return node_load($this->key);
  }
  /**
   * Get object name, unfiltered string
   */
  function name() {
    return $this->value() ? $this->value()->title : parent::name();
  }
  /**
   * Check user access
   */
  function user_access($account) {
    if ($node = $this->value()) {
      node_access('view', $node, $account);
    }
  }
}

/**
 * User objects
 */
class Notifications_User extends Notifications_Drupal_Object {
  public $type = 'user';
  /**
   * Load related object or data
   */
  public function load() {  
    return user_load($this->key);
  }
  /**
   * Get object name, unfiltered string
   */
  function name() {
    return $this->value() ? $this->value()->name : parent::name();
  }
  /**
   * Check user access
   */
  function user_access($account) {
    $user = $this->value();
    return $user && $user->uid &&
    (
      // Always let users view their own profile.
      ($user->uid == $account->uid) ||
      // Administrators can view all accounts.
      user_access('administer users', $account) ||
      // The user is not blocked and logged in at least once.
      ($user->access && $user->status && user_access('access user profiles', $account))
    );
  }
}

/**
 * Check access to an object for user account
 * 
 * @param $type
 *   Object type
 * @param $object
 *   Object or object id
 * @param $account
 *   User account to check access to the object
 */
function notifications_object_access($type, $object, $account) {
  $object = notifications_object_load($type, $object);
  // If object not properly loaded, always false
  if (!$object) {
    return FALSE;
  }
  elseif (($info = notifications_object_type($type)) && ($key = $info['key_field']) && isset($object->$key)) {
    $access = &drupal_static(__FUNCTION__);
    if (!isset($access[$type][$account->uid][$object->$key])) {
      if (isset($info['access callback'])) {
        $access[$type][$account->uid][$object->$key] = _notifications_object_callback($type, 'access callback', $object, $account);
      }
      elseif (isset($info['access'])) {
        $access[$type][$account->uid][$object->$key] = user_access($info['access'], $account);
      }
      else {
        // Not defined, so we allow user access
        $access[$type][$account->uid][$object->$key] = TRUE;
      }
    }
    return $access[$type][$account->uid][$object->$key];
  }
  // If not object information we cannot determine anything 
}

/**
 * Get subscription options for object, account. Only enabled subscription types
 */
function notifications_object_subscribe_options($type, $object, $account = NULL) {
  $account = $account ? $account : $GLOBALS['user'];
  $object = notifications_object_load($type, $object);
  $subscriptions = module_invoke_all('notifications_object_' . $type, 'subscriptions', $object, $account);
  // Filter out subscription types that are disabled
  foreach ($subscriptions as $key => $subs) {
    $type = is_object($subs) ? $subs->type : $subs['type'];
    if (!notifications_subscription_type_enabled($type)) {
      unset($subscriptions[$key]);
    }
  }
  return $subscriptions;
}

/**
 * Build subscribe / unsubscribe options for object
 */
function notifications_object_subscribe_links($type, $object, $account = NULL, $subscribe_options = array(), $unsubscribe_options = array()) {
  $links = array();
  if ($subscriptions = notifications_object_user_subscriptions($type, $object, $account)) {
    foreach ($subscriptions as $index => $subscription) {
      $options = $subscription->is_instance() ? $unsubscribe_options : $subscribe_options;
      if ($link = $subscription->build_link($options)) {
        $links['notifications_' . $index] = $link;
      }
    }
  }
  return $links;
}

/**
 * Get field conditions for this specific object
 */
function notifications_object_condition_fields($type, $object) {
  if ($object = notifications_object_load($type, $object)) {
    // As this does an array_merge_recursive() we get grouped field => array(value1, value2..)
    $fields = module_invoke_all('notifications_object_' .$type, 'conditions', $object);
    // Now we just need to filter out duplicate values
    foreach ($fields as $key => $value) {
      if (is_array($value)) {
        $fields[$key] = array_unique($value);
      }
    }
    return $fields;
  }
}

/**
 * Get list of possible and existing subscriptions for user/object
 * 
 * @param $type
 *   Subscription type to get options: 'user', 'node'
 * @param $object
 *   The object to subscribe. It may be $node or $user
 * @param $account
 *   User account to get options/subscriptions for
 * 
 * @return
 *   Array of subscription options
 *   The enabled ones will have a 'subscriptions' element loaded
 */
function notifications_object_user_subscriptions($type, $object, $account = NULL) {
  $cache = &drupal_static(__FUNCTION__);
  $account = $account ? $account : $GLOBALS['user'];
  $object = notifications_object_load($type, $object);
  
  // Get allowed subscription options for this account to this object
  $subscribe_options = notifications_object_subscribe_options($type, $object, $account);
  $allowed_options = array();
  foreach ($subscribe_options as $option) {
    // So far this is not a subscription but a subscription template
    $subscription = notifications_build_subscription($option);
    $type_key = $subscription->serialize_type();
    // If we have this type cached we don't search more
    if (!isset($cache[$account->uid][$type_key])) {
      if (notifications_user_allowed_subscription($account, $subscription)) {
        $subscription->set_account($account);
        // If anonymous user we don't search more because we cannot find by uid
        if ($account->uid) {
          $find = notifications_get_subscriptions(
            array('uid' => $account->uid, 'type' => $subscription->type),
            $subscription->get_conditions()
          );
          // Allowed subscription type, we store the subscription or the template
          if ($find) {
            $usersubs = current($find);
            $usersubs->name = $subscription->name;
            $subscription = $usersubs;
          }
        }
        $cache[$account->uid][$type_key] = $subscription;
      }
      else {
        // Not allowed subscription type for this user
        $cache[$account->uid][$type_key] = FALSE;
      }
    }
    if ($cache[$account->uid][$type_key]) {
      $allowed_options[] = $cache[$account->uid][$type_key];
    }
  }
  return $allowed_options;
}

/**
 * Get objects to which we can subscribe on current page
 */
function notifications_object_page_objects() {
  $objects = &drupal_static(__FUNCTION__);
  if (!isset($objects)) {
    $objects = messaging_module_invoke_all('notifications_subscription', 'page objects');
  }
  return $objects;
}

/**
 * Build subscriptions for current user to an array of objects
 */
function notifications_object_page_subscriptions($objects) {
  global $user;
  $subscriptions = array();
  foreach ($objects as $type => $object) {
    if ($more = notifications_object_user_subscriptions($type, $object, $user)) {
      $subscriptions = array_merge($subscriptions, $more);
    }
  }
  return $subscriptions;
}

/**
 * Form for object (node, user, term...) subscriptions
 *
 * @param $subscriptions
 *   Array of subscription options
 */
function notifications_object_options_form($form_state, $subscriptions) {
  $form['subscriptions'] =  notifications_object_options_fieldset($subscriptions, FALSE);  
  $form['submit'] = array('#type' => 'submit', '#value' => t('Update'));

  // If full form, redirect so the full page which may have subscription links is updated
  $form['#redirect'] = $_GET['q'];
  // Add always submit callback because the form may have a different name
  $form['#submit'][] = 'notifications_subscriptions_options_form_submit';
  return $form;
}

/**
 * Process submission
 */
function notifications_object_options_form_submit($form, $form_state) {
  $enabled = $disabled = 0;
  // We may have also send method and destination in this form, like on forms from anonymous users
  $send_method = isset($form_state['values']['send_method']) ? $form_state['values']['send_method'] : NULL;
  $destination = isset($form_state['values']['destination']) ? $form_state['values']['destination'] : NULL;
  foreach ($form_state['values']['subscriptions']['options'] as $index => $value) {
    $subscription = $form_state['values']['subscriptions']['params'][$index];   
    if ($value && !$subscription->is_instance()) {
      // We checked a disabled subscription
      if ($send_method) {
        $subscription->send_method = $send_method;
      }
      if ($destination) {
        $subscription->set_destination($destination);
      }
      notifications_save_subscription($subscription);
      $enabled++;
    }
    elseif (!$value && $subscription->is_instance()) {
      // we unchecked an enabled subscription
      notifications_subscription_delete($subscription->sid);
      $disabled++;
    }
  }

  if ($enabled) {
    drupal_set_message(format_plural($enabled, 'A subscription has been created', '@count subscriptions have been created'));
  }
  if ($disabled) {
    drupal_set_message(format_plural($disabled, 'A subscription has been deleted', '@count subscriptions have been deleted'));
  }  
}
