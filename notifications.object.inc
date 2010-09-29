<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Base class for all Notifications entities
 */
abstract class Notifications_Entity {
  protected $type;
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
   * Get object title
   */
  public abstract function get_title();
  /**
   * Get object name
   */
  public abstract function get_name();
  /**
   * Build from db object or template
   */
  public static abstract function build_object($object);
  /**
   * User access function
   */
  public abstract function user_access($account, $op = 'view');
  /**
   * Get information from notifications hooks
   */
  public static abstract function type_info($type = NULL, $property = NULL, $default = NULL);
  /**
   * Get information for specific instance.
   * 
   * This is useful for properties that can be overridden with alter hooks
   */
  public function get_info($property = NULL, $default = NULL) {
    return $this->type_info($this->type, $property, $default);
  }
}

/**
 * Wrapper for Drupal objects
 * 
 * This will be a wrapper for Drupal objects passed around and used as parameters, with some advantages:
 * - All the objects have a 'type' property
 * - When serialized, the object itself won't be serialized
 */
abstract class Notifications_Object {
  // Object type
  public $type;
  public $value;
  public $name;
  protected $object;
  protected $fields;
  /**
   * Constructor
   */
  public function __construct($value = NULL) {
    if (is_numeric($value) || is_string($value)) {
      $this->value = $value;
    }
    elseif (is_object($value) && $value) {
      $this->object = $value;
      $this->value = $this->object_value($value);
    }
  }
  /**
   * Get object type title
   */  
  public abstract function get_title();

  /**
   * Get object name
   */
  public function get_name() {
    if (!empty($this->name)) {
      return $this->name;
    }
    elseif ($this->object()) {
      return $this->object_name($this->object());
    }
    else {
      return t('None');
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
      // We don't know this type, build default object
      $object = new Notifications_Drupal_Object($value);
      $object->type = $type;
      return $object;
    }
  }

  /**
   * Get object unique key as string. The default will be 'type:value'
   */
  public function index() {
    return $this->type . (isset($this->value) ? ':' . $this->value : ':empty');
  }
  /**
   * Get object value
   */
  public function object() {
    if (!isset($this->object)) {
      $object = isset($this->value) ? $this->object_load($this->value) : NULL;
      $this->object = $object ? $object : FALSE;
    }
    return $this->object;
  }
  /**
   * Load related object or data
   */
  public abstract static function object_load($value);
  /**
   * Get name for object
   */
  public abstract static function object_name($object);
  /**
   * Map object to value (key)
   */
  public abstract static function object_value($object);

  /**
   * Check user access. By default it will be true if object can be loaded
   */
  function user_access($account) {
    return (bool)$this->object();
  }
  /**
   * Get fields for this object type
   */
  function get_fields() {
    if (!isset($this->fields)) {
      $this->fields = array();
      if ($object = $this->object()) {
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
   * Get list of possible and existing subscriptions for user/object
   * 
   * Was: notifications_object_user_subscriptions($type, $object, $account = NULL)
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
  function user_subscriptions($account = NULL) {
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
   * Run module_invoke_all('notifications_object_[type]') with this object
   */
  protected function invoke_all($op, $param = NULL) {
    return module_invoke_all('notifications_object_' . $type, $op, $this, $param);    
  }

  /**
   * PHP Magic. Regurn object properties to be serialized
   */
  public function __sleep() {
    return array('type', 'value', 'name');
  }

}

/**
 * Wrapper for generic Drupal objects
 * 
 * This basically implements abstract methods with some defaults
 * 
 * @todo Get some information from entities
 */
class Notifications_Drupal_Object extends Notifications_Object {
  public function get_title() {
    return t('Object');
  }
  /**
   * Load related object or data
   */
  public static function object_load($value) {
    return NULL;
  }
  
  /**
   * Get name for object
   */
  public static function object_name($object) {
    return t('unknown');
  }

  /**
   * Map object to value (key)
   */
  public static function object_value($object) {
    return NULL;
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
  public static function object_load($value) {  
    return node_load($value);
  }
  /**
   * Get object name, unfiltered string
   */
  public static function object_name($node) {
    return $node->title;
  }
  /**
   * Get object key
   */
  public static function object_value($node) {
    return $node->nid;
  }
  /**
   * Check user access
   */
  function user_access($account) {
    if ($node = $this->object()) {
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
  public static function object_load($value) {  
    return user_load($value);
  }
  /**
   * Get object name, unfiltered string
   */
  public static function object_name($object) {
    return $object->name;
  }
  /**
   * Get object key
   */
  public static function object_value($user) {
    return $user->uid;
  }
  /**
   * Check user access
   */
  function user_access($account) {
    $user = $this->object();
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
