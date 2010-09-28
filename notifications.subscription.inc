<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Common base for subscription type and subscription instance
 */
class Notifications_Subscription extends Notifications_Object {
    // Format as plaintext. Note it evaluates to false.
  const FORMAT_PLAIN = 0;
  // Format as html. Note it evaluates to true
  const FORMAT_HTML = 1;
  // Format inline, as a string of csv
  const FORMAT_INLINE = 2;
  // Format as HTML table (4 +1)
  const FORMAT_TABLE = 5;
  // Format as item list (8 + 2(inline) + 1 (html))
  const FORMAT_LIST = 10;
  // Subscription fields
  protected $fields;

  /**
   * Class constructor
   */
  function __construct($template = NULL) {
    if ($template) {
      $properties = (array)$template;
      foreach ($properties as $key => $value) {
        $this->$key = $value;
      }
    }
  }
  public function get_fields() {
    return array();
  }
  /**
   * Build from form submission
   */
  public static function build_submission(&$form_state) {
    // The object may be already created ir it may be an array so we use this function that handles that
    $subscription = notifications_build_subscription($form_state['values']['subscription']);
    // There may be optional fields to add
    if (!empty($form_state['values']['subscription_fields'])) {
      $subscription->set_properties($form_state['values'], array_unique($form_state['values']['subscription_fields']));
    }
    return $subscription;
  }
  
  /**
   * Save fields from form submission
   */
  public function set_properties($values, $fields) {
    foreach ($fields as $key) {
      if (isset($values[$key])) {
        switch ($key) {
          case 'fields':
            if (!empty($values['parsed_fields'])) {
              $this->set_fields($values['parsed_fields']);
            }
            else {
              $this->set_fields($values['fields']);
            }
            break;
          case 'destination':
            $this->set_destination($values[$key]);
            break;
          default:
            $this->$key = $values[$key];
            break;         
        }
      }
    }
  }
 
  /**
  * Validate form submission
  */
  public static function validate_submission(&$form_state) {
    // The object may be already created ir it may be an array so we use this function that handles that
    $subscription = notifications_build_subscription($form_state['values']['subscription']);
    // There may be optional fields to add
    if (!empty($form_state['values']['subscription_fields'])) {
      foreach ($form_state['values']['subscription_fields'] as $field) {
        if (isset($form_state['values'][$field])) {
          $field_value = $form_state['values'][$field];
          switch ($field) {
            case 'fields':
              notifications_include('object.inc');
              foreach ($field_value as $key => $field_values) {
                if ($value = notifications_field_real_value($field_values['type'], $field_values['edit'])) {
                  $form_state['values']['parsed_fields'][$key] = array('type' => $field_values['type'], 'value' => $value);
                }
                else {
                  form_set_error("$field]['edit", t('Invalid value for this field.'));
                  break;
                }
              }
              break;
            case 'send_method':
              // There may be other destination validation running that sets the destination object before
              if (empty($form_state['values']['destination']) && (!$subscription->mdid || $subscription->send_method != $field_value)) {
                if ($subscription->validate_destination($field_value)) {
                  $form_state['values']['subscription_fields'][] = 'destination';
                }
                else {
                  form_set_error('send_method', t('Cannot create a valid destination.'));
                }
              }
              break;
          }   
        }
      }
    }
  }
  
}

/**
 * Subscription type
 */
class Notifications_Subscription_Type extends Notifications_Subscription {
  // Subscription type name
  public $type;
  // Instance class
  public $instance = 'Notifications_Subscription_Instance';
  // Event that triggers this subscription
  public $event_type;
  // Condition fields
  public $conditions;

  /**
   * Build subscription type
   */
  public static function build_type($type) {
    $info = notifications_subscription_type($type);
    $class = !empty($info['class']) ? $info['class'] : 'Notifications_Subscription_Type';
    $subscription = new $class($info);
    $subscription->type = $type;
    return $subscription;
  }
  /**
   * Build instance of this type
   */
  public function build_instance($object) {
    $class = $this->instance;
    $subscription = new $class($object);
    $subscription->type = $this->type;
    return $subscription;
  }
  /**
   * Get field types. The order is important as it will determine the field index
   */
  function field_types() {
    return array();
  }
  /**
   * Get object types
   */
  function object_types() {
    return array();
  }
  /**
   * Get fields as array of field objects
   */
  function get_fields() {
    if (!isset($this->fields)) {
      $this->set_fields();
    }
    return $this->fields;
  }
  /**
   * Set fields
   */
  function set_fields($fields = NULL) {
    if ($fields) {
      foreach ($fields as $field) {
        $field->subscription = $this;
        $this->fields[$field->index] = $field;
      }
    }
  }
}

/**
 * Notifications Subscription instance
 */
class Notifications_Subscription_Instance extends Notifications_Subscription {
  // Blocked subscriptions, for blocked users
  const STATUS_BLOCKED = 0;
  // Enabled ones, will produce notifications
  const STATUS_ACTIVE = 1;
  // Temporarily disabled ones, maybe user on holidays
  const STATUS_INACTIVE = 2;
  // Disabled because the subscription type is disabled
  const STATUS_DISABLED = -1;

  // This class inherits some properties from Messaging_Object
  // public $uid;
  // public $language;
  
  // Unique subscription id
  public $sid = 0;
  // Destination id
  public $mdid;
  // Subscription type
  public $type;
  // Event that triggers this subscription
  public $event_type;
  // Number of conditions that must be met
  public $conditions;
  // Values to pass on to the queue for composition
  public $send_interval;
  public $send_method;
  public $cron = 1;
  public $module = 'notifications';
  // Subscription status, defaults to active
  public $status = 1;
  // The address for the destination (Unused, just for logging. To be obsoleted)
  public $destination = '';


  // Temporary variables, not saved to the DB
  protected $destination_object;
  protected $format;
  protected $objects;
  // Name for this subscription that will be quite dependent on context
  public $name;
  // Mark if incomplete loading of objects
  public $incomplete;
  // Mark when editable
  public $editable;
  // Temporary error message, to display when validation fails
  public $error_message;
  
  /**
   * Class constructor
   */
  function __construct($object = NULL) {
    parent::__construct($object);  
    // Default values for new objects
    if (empty($this->sid)) {
      $this->status = self::STATUS_ACTIVE;
      $this->created = time();
    }
  }
  /**
   * Build from type, template
   */
  public static function build_type($type, $template = NULL) {
    return notifications_subscription($type)->build_instance($template);
  }
  /**
   * Build for a subscription type
   * 
   * @param $template
   *   Template object or subscription type
   */
  public static function build_object($template) {
    return notifications_subscription($template->type)->build_instance($template);
  }


 
  /**
   * Produce fieldset to edit field values
   */
  function fields_subform() {
    $elements = array(
      '#type' => 'fieldset',
      '#tree' => TRUE,
      '#description' => $this->get_description(),
    );
    foreach ($this->get_type_fields() as $index => $field) {
      $elements[$index]['type'] = array('#type' => 'value', '#value' => $field->field);
      $elements[$index]['value'] = array('#type' => 'value', '#value' => $field->value);
      $elements[$index]['edit'] = $this->field_element($field->field, $field->value, TRUE, TRUE);
    }
    return $elements;
  }
  
  /**
   * Build subscribe /unsubscribe link
   */
  function build_link($options = array()) {
    if ($this->is_instance()) {
      // Unsubscribe link
      $title = t('Unsubscribe from: @name', array('@name' => $this->get_name()));
      $options += array('sid' => $this->sid);
      $props = notifications_subscription_get_link('unsubscribe', $this, $options);
    }
    else {
      // Subscribe link
      $title = t('Subscribe to: @name', array('@name' => $this->get_name()));
      $options += array('type' => $this->type, 'fields' => $this->get_conditions());
      $props = notifications_subscription_get_link('subscribe', $this, $options);
    }
    return $props ? array(
      'title' => $title,
      'html' => TRUE,
      'href' => $props['href'],
    ) + $props['options'] : NULL;
  }

  /**
   * Set user account as the owner of this subscription and take care of defaults for this account.
   * 
   * @param $account
   *   User account or user uid
   */
  function set_account($account) {
    $account = messaging_user_object($account);
    $this->set_user($account);
    if (!isset($this->send_interval)) {
      $this->send_interval = notifications_user_setting('send_interval', $account, 0);
    }
    if (!isset($this->send_method)) {
      $this->send_method = notifications_user_setting('send_method', $account);
    }
    if (empty($this->language)) {
      $this->set_language(); 
    }
    // Return TRUE if all parameters set
    return isset($this->uid) && isset($this->send_interval) && isset($this->send_method);
  }

  /**
   * Get user account
   */
  function get_account() {
    return $this->get_user();
  }
  /**
   * Load from db
   * 
   * @param $sid
   *   Subscription id 
   * @param $full
   *   Whether to load all fields in the same operation
   */
  public static function load($sid) {
    return notifications_subscription_load($sid);
  }
  /**
   * Load condition fields from db
   */
  function load_fields() {
    $this->fields = array();
    if (!empty($this->sid)) {
      $result = db_query("SELECT * FROM {notifications_fields} WHERE sid = %d", $this->sid);
      while ($field = db_fetch_object($result)) {
        $this->set_field($field);
      }      
    }
  }
  /**
   * Save to db
   */
  function insert() {
    $result = parent::insert();
    $this->save_fields(FALSE);
    return $result; 
  }
  /**
   * Update db
   */
  function update() {
    $this->save_fields(TRUE);
    return parent::update();
  }
  /**
   * Delete from db
   */
  function delete() {
    if ($this->is_instance()) {
      module_invoke_all('notifications_subscription', 'delete', $this);
      db_query("DELETE FROM {notifications_fields} WHERE sid = %d", $this->sid);
      return parent::delete();
    }
  }
  /**
   * Check subscription user account and related parameters
   */
  function check_account() {
    if (!isset($this->uid)) {
      return FALSE;
    }
    elseif (!isset($this->send_method) || !isset($this->send_interval)) {
      return $this->set_account($this->get_user());
    }
    else {
      return TRUE;
    }
  }
  
  /**
   * Check destination or create it if new
   */
  function check_destination() {
    if (empty($this->mdid)) {
      return $this->create_destination();
    }
    elseif ($destination = $this->get_destination()) {
      // We may need to create a new destination if this address is not valid for method has changed
      if (!empty($this->send_method) && messaging_method_info($this->send_method, 'address_type') != $destination->type) {
        return $this->create_destination();
      }
      else {
        return $destination;
      }
    }
    else {
      // Last try, figure out a destination
      return $this->create_destination();
    }
  }
  /**
   * Check permission for user account
   */
  function check_access($account = NULL) {
    $account = $account ? $account : $this->get_account();
    return notifications_user_allowed_subscription($account, $this);
  }
  /**
   * Create destination for this subscription
   */
  function create_destination($method = NULL, $address = NULL) {
    return $this->validate_destination($method, $address, TRUE);
  }
  /**
   * Validate destination for this subscription
   */
  function validate_destination($method = NULL, $address = NULL, $create = FALSE) {
    $account = $this->get_user();
    if ($method && $address) {
      if ($create) {
        $destination = Messaging_Destination::create_method($method, $address, $account->uid);
        $this->set_destination($destination);
        return $destination;
      }
      else {
        // Just validate
        return Messaging_Destination::validate_method($method, $address, $account->uid);
      }
    }
    elseif ($account->uid) {
      // For registered users we have other ways to create a destination
      if ($method) {
        if ($address = messaging_user_destination($account, $method)) {
          // We have a method and address for this user account, fine
          return $this->validate_destination($method, $address, $create);
        }
      }
      else {
        // We still can have a send method or get it from account
        $method = !empty($this->send_method) ? $this->send_method  : messaging_method_default($account);
        if ($method) {
          return $this->validate_destination($method, NULL, $create);
        }
      }
    }    
  }

  /**
   * Save condition fields to db
   * 
   * @param $update
   *   Whether this is an old subscription being created
   */
  function save_fields($update = FALSE) {
    if (isset($this->fields)) {
      if ($update) {
        db_query("DELETE FROM {notifications_fields} WHERE sid = %d", $this->sid);
      }
      foreach ($this->fields as $field) {
        db_query("INSERT INTO {notifications_fields} (sid, field, value, intval) VALUES(%d, '%s', '%s', %d)", $this->sid, $field->field, $field->value, (int)$field->value);
      }
    }
  }
  /**
   * Add a new condition, update counter
   */
  function add_condition($name, $value) {
    $this->add_field($name, $value);
    $this->conditions++;
  }
  /**
   * Get fields as array of field => value pairs
   * 
   * Duplicate fields are returned as field => array(value1, value2...)
   * 
   * @param $type
   *   Optional to just return the values for some field type
   */
  function get_conditions($type = NULL) {
    $list = array();
    foreach ($this->get_fields() as $field) {
      // We cannot simply use isset() because the value may be NULL
      if (!array_key_exists($field->field, $list)) {
        $list[$field->field] = $field->value;
      }
      elseif (is_array($list[$field->field])) {
        $list[$field->field][] = $field->value;
      }
      else {
        $list[$field->field] = array($list[$field->field], $field->value);
      }
    }
    if (isset($type)) {
      return isset($list[$type]) ? $list[$type] : NULL;
    }
    else {
      return $list;
    }
  }
  /**
   * Check whether we have a given condition
   */
  function has_condition($type, $value) {
    $conds = $this->get_conditions($type);
    return isset($conds) && (is_array($conds) && in_array($value, $conds) || !is_array($conds) && $conds === $value);
  }

  /**
   * Order and serialize fields so we can get a unique signature for this subscription fields
   */
  function serialize_fields() {
    return notifications_array_serialize($this->get_conditions());
  }
  /**
   * Serialize type and conditions
   */
  function serialize_type() {
    return implode('/', array($this->type, $this->serialize_fields()));
  }
  /**
   * Add a new field with name, value
   */
  function add_field($type, $value = NULL) {
    $this->set_field($this->build_field($type, $value));
  }
  /**
   * Add field arguments from url
   * 
   * @param $fields
   *   String of field names separated by commas
   * @param $values
   *   String of field values separated by commas
   */
  function add_field_args($fields, $values) {
    $names = explode(',', $fields);
    $params = explode(',', $values);
    foreach ($names as $index => $type) {
      $this->add_field($type, isset($params[$index]) ? $params[$index] : NULL);
    }    
  }
  
  /**
   * Set a field. 
   * - If the field has already a key (index) it will replace an existing one with the same key
   * - If not key, it will be added to the field list with an ordinary key generated
   * 
   * @param $field
   *   Field object with one or more of these properties: key, type, field, value
   */
  function set_field($field, $replace = FALSE) {
    if (!isset($field->key) || !$replace) {
      $field->key = isset($this->fields) ? count($this->fields) : 0;
    }
    // Add to the fields array, may replace an old one
    $this->fields[$field->key] = $field;
  }
  /**
   * Get fields as array of field objects
   */
  function get_fields() {
    if (!isset($this->fields)) {
      if ($this->is_instance()) {
        $this->load_fields();
      }
      else {
        $this->set_fields();
      }
    }
    return $this->fields;
  }
  /**
   * Check all fields are there and optinally that they have a value
   */
  function check_fields($check_values = TRUE) {
    $this->set_fields();
    $instance_fields = array();
    foreach ($this->get_fields() as $field) {
      if ($check_values && !isset($field->value)) {
        return FALSE;
      }
      else {
        $instance_fields[] = $field->field;
      }
    }
    $type_fields = array();
    foreach ($this->get_type_fields() as $field) {
      $type_fields[] = $field->field;
    }
    if (count($instance_fields) != count($type_fields)) {
      return FALSE;
    }
    else {
      // Set the number of conditions if not set 
      if (!isset($this->conditions)) {
        $this->conditions = count($instance_fields);
      }      
      // Order the arrays so we can compare them. Note that we can have the same field name more than once
      asort($instance_fields);
      asort($type_fields);
      return array_values($instance_fields) == array_values($type_fields);
    }
  }
  /**
   * Set field values, all at a time
   * 
   * Each field item may have different formats:
   * - Numeric key => field type. Like array(nid, tid)
   * - Numeric key => array('type' => type, 'value' => value)
   * - Numeric key => Object ($field object with field, value properties)
   * @param $fields
   *   Fields is an array of field items, each of them with many possible formats:
   *   NULL to set the fields from the subscription type or template if not set
   */
  function set_fields($fields = NULL, $normalize = TRUE) {
    if (isset($fields)) {
      $this->fields = array();
      $this->add_fields($fields, $normalize);
    }
    elseif (!isset($this->fields)) {
      // Set the fields from the subscription type or template
      $this->fields = array();
      $this->add_fields($this->get_type_fields(), FALSE);
    }
  }
  /**
   * Add field values to existing ones
   */
  function add_fields($fields, $normalize = TRUE) {
    if ($normalize) {
      $fields = $this->normalize_fields($fields);
    }
    foreach ($fields as $key => $value) {
      $this->set_field($value);
    }    
  }
  /**
   * Get fields from subscription type as normalized array of objects
   */
  function get_type_fields() {
    return $this->normalize_fields($this->get_type('fields'));
  }
  /**
   * Convert fields to a standard format: arrray of object fields
   */
  public static function normalize_fields($fields) {
    $normal = array();
    if ($fields && is_array($fields)) {
      foreach ($fields as $key => $value) {
        if (is_object($value)) {
          $normal[] = $value;
        }
        elseif (is_array($value)) {
          // Object or Array with type, value keys
          $normal[] = self::build_field($value['type'], $value['value']);
        }
        elseif (is_numeric($key) && is_string($value)) {
          // In this case the value is the field type
          $normal[] = self::build_field($value, NULL);
        }
        else {
          // Default, key should be field type, value may be NULL
          $normal[] = self::build_field($key, $value);
        }
      }
    }
    return $normal;    
  }
  /**
   * Build field from type, value
   */
  public static function build_field($type, $value) {
    return (object)array(
      'field' => $type,
      'value' => $value,
    );
  }
  /**
   * Get destination object
   */
  function get_destination() {
    if (!isset($this->destination_object)) {
      $this->destination_object = !empty($this->mdid) ? Messaging_Destination::load($this->mdid) : FALSE;
    }
    return $this->destination_object;
  }

  /**
   * Get language object
   */
  function get_language() {
    if (!isset($this->_language)) {
      if (!empty($this->language) && ($languages = language_list()) && isset($languages[$this->language])) {
        $this->_language = $languages[$this->language];
      }
      else {
        $this->_language = user_preferred_language($this->get_account());
        $this->language = $this->_language->language;
      }
    }
    return $this->_language;
  }

  /**
   * Set destination object
   */
  function set_destination($destination) {
    if (empty($destination)) {
      $this->mdid = 0;
      $this->destination = '';
      $this->destination_object = FALSE;
    }
    elseif (is_object($destination)) {
      $this->uid = $destination->uid;
      $this->mdid = $destination->mdid;
      $this->destination = $destination->address;
      $this->destination_object = $destination;
    }
    elseif (is_numeric($destination)) {
      $this->mdid = $destination;
    }
  }
  /**
   * Get subscription type data
   */
  function get_type($property = NULL) {
    return empty($this->type) ? NULL : notifications_subscription_types($this->type, $property);
  }

  /**
   * Whether this subscription's fields are editable or not
   * 
   * Unless preset the 'editable' property, this is how it works:
   * - Once we have an instance we don't allow changing the fields, which may cause some consistency problems
   * - Also if the subscription type has no fields, this is not editable
   * - When it has fields and they've been all preset, not editable either
   */
  function is_editable() {
    if (!isset($this->editable)) {
      if (!$this->is_instance() && ($type_fields = $this->get_type_fields())) {
        // It is editable if not all fields are set
        $this->editable = count($type_fields) > count($this->get_instance_fields());
      }
      else {
        // It is instance or the type has no fields
        $this->editable = FALSE; 
      }
    }
    return $this->editable;
  }
  /**
   * Get editable fields. They're the ones in the subscription type that have no value
   */
  function get_editable_fields() {
    return $this->filter_fields($this->get_type_fields(), FALSE);
  }
  /**
   * Get instance fields that have a value set
   */
  function get_instance_fields() {
    return $this->filter_fields($this->get_fields());
  }
  /**
   * Filter out fields that have no value set ($isset = FALSE to get the ones that are set)
   */
  public static function filter_fields($fields, $isset = TRUE) {
    foreach ($fields as $key => $field) {
      if ($isset && !isset($field->value) || !$isset && isset($field->value)) {
        unset($fields[$key]);
      }
    }
    return $fields;    
  }

  /**
   * Get instance of this one for certain conditions
   * 
   * @param $params
   *   Parameters to add to the subscription type to get an instance of itself
   * @param $return_self
   *   Return itself if no instance found
   */
  function get_instance($params = array(), $return_self = FALSE) {
    $params += array('type' => $this->type);
    $cache = &drupal_static('Notifications_Subscription_instances');
    $cache_key = notifications_array_serialize($params);
    if (!isset($cache[$cache_key])) {
      $subs = notifications_get_subscriptions($params);
      $cache[$cache_key] = $subs ? current($subs) : FALSE;      
    }
    return $cache[$cache_key] ? $cache[$cache_key] : ($return_self ? $this : NULL);
  }

  /**
   * Load subscription objects
   */
  function load_objects() {
    if (!isset($this->objects)) {
      $this->objects = array();
      foreach ($this->get_fields() as $field) {
        if ($type = notifications_field_type($field->field, 'object_type')) {
          if ($object = notifications_object_load($type, $field->value)) {
            if (!isset($this->objects[$type])) {
              $this->objects[$type] = $object;
            }
            elseif (is_array($this->objects[$type])) {
              // Was an array, just add
              $this->objects[$type][] = $object;
            }
            else {
              // Was single element, make into an array with this new object
              $this->objects[$type] = array($this->objects[$type], $object);
            }
          }
          else {
            // Object cannot be loaded, mark as incomplete
            $this->incomplete = TRUE;
          }
        }
      } 
    }
    return empty($this->incomplete);
  }
  /**
   * Get objects
   */
  function get_objects() {
    $this->load_objects();
    return $this->objects;
  }

  /**
   * Format as short text
   */
  function format_short($format = self::FORMAT_HTML) {
    return t('@type: !values', array('@type' => $this->get_type('title'), '!values' => $this->format_name($format | self::FORMAT_INLINE)));
  }
  /**
   * Format as long text
   */
  function format_long($format = self::FORMAT_HTML) {
    return t('Subscription %id of type %type to: !values', array('%id' => $this->sid, '%type' => $this->get_type('title'), '!values' => $this->format_name($format | self::FORMAT_INLINE)));
  }
  
  /**
   * Get subscription short name.
   */
  function get_name($format = self::FORMAT_HTML) {
    if (!isset($this->name)) {
      // This subscription doesn't have a name, we'll find something
      $this->name = $this->format_name($format | self::FORMAT_INLINE);
    }
    return $this->name;
  }
  /**
   * Get description
   */
  function get_description($format = self::FORMAT_HTML) {
    return $this->get_type('description');
  }
  // Get title
  function get_title() {
    return $this->get_type('title');
  }   
  /**
   * If the subscription type has a name, like custom subscriptions have, that will be the name
   * Otherwise we build the name using fields and values
   */
  function format_name($format = self::FORMAT_PLAIN) {
    if ($name = $this->get_type('name')) {
      return $name;
    }
    else {
      return $this->format_fields($format);
    }   
  }
  
  /**
   * Format all fields
   *
   * @return array();
   *   Array of arrrays with (name, value)
   */
  function format_fields($format = self::FORMAT_HTML) {
    if (!isset($this->format[$format]['fields'])) {
      // Get field names and values formatting each field
      $items = array();
      foreach ($this->get_fields() as $field) {
        $items[] = $this->format_field($field, $format);
      }
      $this->format[$format]['fields'] = $this->format_items($items, $format);
    }
    return $this->format[$format]['fields'];
  }
  
  /**
   * Format items
   *
   * @param $items
   *   Array of arrays with 'name' and 'value' elements
   */
  function format_items($items, $format = self::FORMAT_INLINE) {
    // If no items the output will be always an empty string
    if (!$items)  {
      return '';
    }
    // Some formats need each item to be a string first 
    if ($format & self::FORMAT_INLINE) {
      foreach ($items as $key => $value) {
        if (is_array($value)) {
          $items[$key] = implode(': ', $value);
        }
      }
    }
    switch (TRUE) {
      case $format & self::FORMAT_INLINE:
        return implode(',', $items);
      case $format & self::FORMAT_LIST:
        return theme('item_list', $items);
      case $format & self::FORMAT_TABLE:
        return theme('table', array(), $items);

      default:
        // Items not formatted, return as array
        return $items;
    }
  }
  /**
   * Format subscriptions field for display and get some more information
   * 
   * @return array()
   *   Array with 'name' and 'value' elements
   */
   function format_field($field, $format = self::FORMAT_HTML) {
    notifications_include('object.inc');

    $type = $field->field;
    $value = $field->value;
    
    $format_name = notifications_field_format_name($type);
    $format_value = notifications_field_format_value($type, $value, $format & self::FORMAT_HTML, NULL);
    
    return $format & self::FORMAT_INLINE ? $format_name . ': ' . $format_value : array('name' => $format_name, 'value' => $format_value);
  }

  /**
   * Subscription information field for several forms
   * 
   * @return Forms API field structure
   */
  function form_info() {
    $info = $this->get_type();
    // Get fields formatted as array of items
    $fields = $this->get_fields();
    if (!empty($info['name'])) {
      // This subscription type already has a name
      $value = $info['name'];
    }
    elseif (empty($fields)) {
      // No name, maybe no fields it should be enough with the title
      $value = '';
    }
    elseif (count($fields) == 1) {
      // If the field is unique, we don't need a table nor a name for it
      $value = $this->format_fields(self::FORMAT_HTML | self::FORMAT_INLINE);
    }
    else {
      // Multiple fields, format as a table
      $value = $this->format_fields(self::FORMAT_TABLE);
    }
    // Build a form field with all these values
    $field = array(
      '#type' => 'item',
      '#title' => t('@type subscription', array('@type' => $this->get_type('title'))),
      '#value' => $value,
    );
    if (!empty($info['description'])) {
      $field['#description'] = $info['description'];
    }
    return $field;
  }

  /**
   * Display a form field for a notifications_field
   */
  public function field_element($type, $value = NULL, $title = FALSE, $required = FALSE ) {
    notifications_include('object.inc');
    return notifications_field_form_element($type, $value, $this, $title, $required);
  }

  /**
   * Status list
   */
  public static function status_list() {
    return array(
      self::STATUS_ACTIVE => t('active'),
      self::STATUS_BLOCKED => t('blocked'),
      self::STATUS_INACTIVE => t('inactive'),
      self::STATUS_DISABLED => t('disabled'),
   );
  }

  /**
   * Magic method, set protected properties
   * 
   * Warning: This also runs for unset properties
   */
  public function __set($name, $value) {
    switch ($name) {
      case 'fields':
        $this->set_fields($this->normalize_fields($value));
        break;
      case 'destination':
        if (is_object($value)) {
          $this->set_destination($value);
        }
        else {
          $this->destination = $value;
        }
        break;
      default:
        parent::__set($name, $value);
    }
  }
  /**
   * Magic method, get protected properties
   */
  public function __get($name) {
    switch ($name) {
      case 'fields':
        return $this->get_conditions();
        break;
    }
  }

}

/**
 * List of subscriptions or subscription types
 */
class Notifications_Subscription_List {
  public $subscriptions = array();
  protected $instances = array();
  protected $types = array();
  /**
   * Construct with list of subscriptions
   */
  public function __construct() {
    if ($args = func_get_args()) {
      $this->add($args);
    }
  }
  /**
   * Add subscription/s to the list
   */
  public function add($items) {
    if (is_array($items)) {
      foreach ($items as $item) {
        $this->add($item);
      } 
    }
    else {
      $this->add_subscription($subscription);
    }
  }
  /**
   * Add single subscription
   */
  function add_subscription($subscription) {
    $this->subscriptions[] = $subscription;
    $this->types[$subscription->type][] = $subscription;
    if ($subscription->is_instance()) {
      $this->instances[$subscription->sid] = $subscription;
    }
  }
  
  /**
   * Subform with subscription options so it can be reused for a fieldset on a bigger form
   * 
   * Was: notifications_object_options_subform($subscriptions, $buttons = TRUE)
   * 
   * @param $subscriptions
   *   List of subscription objects
   * @param $buttons
   *   Whether to add buttons to the fieldset
   */
  function options_subform($buttons = TRUE) {
    $form['subscriptions'] = $this->options_fieldset(TRUE);
    $form['subscriptions'] += array(
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,  
    );
    if ($buttons) {
      $form['subscriptions']['submit'] = array('#type' => 'submit', '#value' => t('Update'));
      // If full form, redirect so the full page which may have subscription links is updated
      $form['#redirect'] = $_GET['q'];
    }
    $form['#submit'][] = 'notifications_subscriptions_options_form_submit';
    return $form;
  }

  
  /**
   * Build fieldset with subscription options
   * 
   * Was: notifications_object_options_fieldset($subscriptions, $title = FALSE)
   */
  function options_fieldset($title = FALSE) {
    $elements = array(
      '#tree' => TRUE,
    );
    // Process all options building the array of indexed params for each
    $options = $params = $defaults = array();
    $index = 1; // Index to map checkboxes to subscriptions
    $number = 0; // Number of active subscriptions
    foreach ($this->subscriptions as $subscription) {
      $options[$index] = $subscription->get_name();
      $params[$index] = $subscription;
      // Check wether user is subscribed
      if ($subscription->is_instance()) {
        $defaults[] = $index;
        $number++;
      }
      $index++;    
    }
    $elements['params'] = array('#type' => 'value', '#value' => $params);
    $elements['options'] = array(
      '#type' => 'checkboxes',
      '#default_value' => $defaults,
      '#options' => $options,
    );
    if ($title) {
      $elements['#title'] = t('Subscriptions (@number)', array('@number' => $number));
    }
    return $elements;
  }
}


/**
 * Main Subscription form builder
 */
function notifications_subscription_form(&$form_state, $subscription, $destination_path = '') {
  module_load_include('pages.inc', 'notifications');
  return notifications_subscription_base_form($form_state, $subscription, $destination_path);
}

/**
 * Main subscription form validate
 */
function notifications_subscription_form_validate($form, &$form_state) {
  module_load_include('pages.inc', 'notifications');
  return notifications_subscription_base_form_validate($form, $form_state);
}

/**
 * Main subscriptions form submit
 */
function notifications_subscription_form_submit($form, &$form_state) {
  module_load_include('pages.inc', 'notifications');
  return notifications_subscription_base_form_submit($form, $form_state);
}

/**
 * Form with multiple subscription options
 */
function notifications_subscriptions_options_form($form_state, $subscriptions, $account = NULL, $destination = NULL, $length = 40) {
  $account = $account ? $account : $GLOBALS['user'];
  notifications_include('object.inc');
  $form = notifications_object_options_form($form_state, $subscriptions);
  // If anonymous account, add destination subform
  if (!$account->uid) {
    $form = notifications_subscription_destination_subform($form, $account, $destination = NULL, NULL, $length);
  }
  return $form;
}

/**
 * Subform with multiple subscription options
 */
function notifications_subscriptions_options_subform($subscriptions, $buttons = TRUE) {
  notifications_include('object.inc');
  return notifications_object_options_subform($subscriptions, $buttons);
}
/**
 * Wrapper submit callback se we can do an include before actual submission
 */
function notifications_subscriptions_options_form_submit($form, &$form_state) {
  notifications_include('object.inc');
  return notifications_object_options_form_submit($form, $form_state);
}

/**
 * Add destination subform
 */
function notifications_subscription_destination_subform($form, $account, $destination = NULL, $send_methods = NULL, $length = 40) {
  notifications_include('destination.inc');
  $form['send_method'] = notifications_destination_address_subform($account, $destination, $send_methods, $length);
  // Add our validate and submit callbacks first
  $form['#validate'] = array_merge(array('notifications_subscription_destination_subform_validate'), isset($form['#validate']) ? $form['#validate'] : array());
  $form['#submit'] = array_merge(array('notifications_subscription_destination_subform_submit'), isset($form['#submit']) ? $form['#submit'] : array());
  return $form;
}

/**
 * Validate submitted destination
 */
function notifications_subscription_destination_subform_validate($form, &$form_state) {
  notifications_include('destination.inc');
  // Get destination from form and check it is ok for an anonymous user
  $dest = notifications_destination_parse_submitted($form_state['values'], TRUE);
  if (!empty($dest['method']) && !empty($dest['address'])) {
    // Set destination field so we don't validate again send method later
    $form_state['values']['destination'] = TRUE;
    // Set send method in case we need some more validation for the subscription
    $form_state['values']['send_method'] = $dest['method'];
  }
}

/**
 * Create submitted destination
 */
function notifications_subscription_destination_subform_submit($form, &$form_state) {
  notifications_include('destination.inc');
  // Get destination from form and add method / destination to form values
  $dest = notifications_destination_parse_submitted($form_state['values'], FALSE);
  if ($destination = Messaging_Destination::create($dest)) {
    $form_state['values']['send_method'] = $dest['method'];
    $form_state['values']['destination'] = $destination;
    $form_state['values']['subscription_fields'][] = 'send_method';
    $form_state['values']['subscription_fields'][] = 'destination';
  }
  else {
    // We should have trapped this on validation. Just in case print an error message.
    drupal_set_message(t('Cannot create a destination for this subscription.'), 'error');
  }
}

/**
 * Get subscribe / unsubscribe page link for subscription
 * 
 * @param $op
 *   Operation: 'subscribe', 'unsubscribe'
 * @param $subscription
 *   Subscription object, may be instance or not
 * @param $options
 *   Link options
 */
function notifications_subscription_get_link($op, $subscription, $options = array()) {
  $options += array('destination' => $_GET['q']);
  if ($op == 'subscribe' && variable_get('notifications_ui_subscribe_links', 0)) {
    $options += array('signed' => TRUE, 'confirm' => FALSE);
  }
  if ($op == 'unsubscribe' && variable_get('notifications_ui_unsubscribe_links', 0)) {
    $options += array('signed' => TRUE, 'confirm' => FALSE);
  } 
  return notifications_build_link($op, $options, 'subscription', $subscription);
}


/**
 * Check subscription parameters
 */
function notifications_check_subscription($subscription) {
  if (!$subscription->check_account()) {
    $subscription->error_message = t('Invalid user account for this subscription.');
    return FALSE;
  }
  elseif (!$subscription->check_fields()) {
    $subscription->error_message = t('Invalid field values for this subscription.');
    return FALSE;
  }
  elseif (!$subscription->check_destination()) {
    $subscription->error_message = t('The destination method or address for the subscription is not valid.');
    return FALSE;
  }
  else {
    return TRUE;
  }
}

/**
 * Get an individual subscription.
 *
 * @param $subs
 *   Either a subscription object or a subscription id (sid).
 * @param $refresh
 *   Force cache refresh
 * @return
 *   Subscriptions object.
 */
function notifications_load_subscription($subs, $refresh = FALSE) {
  $subscriptions = &drupal_static(__FUNCTION__);
  if (is_object($subs)) {
    if(is_a($subs, 'Notifications_Subscription')) {
      $subscriptions[$subs->sid] = $subs;
    }
    else {
      $subscriptions[$subs->sid] = Notifications_Subscription::build($subs);
    }
    return $subscriptions[$subs->sid];
  }
  else {
    if ($refresh || !$subscriptions || !array_key_exists($subs, $subscriptions)) {
      $subscriptions[$subs] = Notifications_Subscription::load($subs);
    }
    return $subscriptions[$subs];
  }
}

/**
 * Delete subscription and clean up related data, included the static cache
 * 
 * It also removes pending notifications related to that subscription 
 * 
 * @param $sid
 *   Subscription object or sid or array of sids of subscription/s to delete
 */
function notifications_subscription_delete($sid) {
  $sid = is_object($sid) ? $sid->sid : $sid;
  $cache = &drupal_static('notifications_load_subscription');

  if (is_array($sid)) {
    $where = 'sid IN (' . db_placeholders($sid) . ')';
    if ($cache) {
      $cache = array_diff_key($cache, array_flip($sid));;
    }
  }
  else {
    $where = "sid = %d";
    $cache[$sid] = NULL;
  }
  foreach (array('notifications', 'notifications_fields') as $table) {
    db_query("DELETE FROM {". $table ."} WHERE " . $where, $sid);
  }
  // Delete queued notifications for this subscription
  notifications_queue()->queue_delete(array('sid' => $sid));
}

/**
 * Delete multiple subscriptions and clean up related data (pending notifications, fields).
 * 
 * Warning: If !$limit, it will delete also subscriptions with more conditions than the fields passed.
 * 
 * @param array $params
 *   Array of multiple conditions in the notifications table to delete subscriptions
 * @param array $field_conditions
 *   Array of multiple conditions in the notifications_fields table to delete subscriptions
 * @param $limit
 *   Whether to limit the result to subscriptions with exactly that condition fields
 *   
 * @return int
 *   Number of deleted subscriptions
 */
function notifications_delete_subscriptions($params, $field_conditions = array(), $limit = FALSE) {
  notifications_include('query.inc');

  // For exact condition fields we add one more main condition.
  if ($limit) {
    $params += array('conditions' => count($field_conditions));
  }
   // This will get partially loaded objects, only with sid field
  $query['select'][] = 's.sid';
  $subscriptions = notifications_query_subscriptions($params, $field_conditions, $query, FALSE);

  // This is the actual deletion, pass the array of sids
  if ($subscriptions) {
    notifications_subscription_delete(array_keys($subscriptions));
  }
  return $subscriptions ? count($subscriptions) : 0;
}


/**
 * Get subscriptions that match a set of conditions.
 *
 * @param $main_conditions
 *   Conditions for the main notifications table
 * @param $field_conditions
 *   Optional array of condition fields. The elements may be
 *   - single field => value pairs 
 *   - or numeric key => array('type' => field, 'value' => value)
 * @param $exact_fields
 *   Whether to limit the result to subscriptions with exactly that condition fields. Otherwise we
 *   look for subscriptions that have and match that fields but may have more than that.
 * @param $key
 *   Optional key field to use as the array index. Will default to sid 
 *   For notifications with one field, it may be 'value' or 'intval'
 * @param $pager
 *   Whether to throw a pager query 
 * @return
 *   Array of subscriptions indexed by uid, module, field, value, author
 * 
 * @todo Check field types for building the query
 */
function notifications_get_subscriptions($main_conditions, $field_conditions = NULL, $exact_fields = TRUE, $key = 'sid', $pager = NULL) {
  notifications_include('query.inc');
  // Build query conditions using the query builder.  
  $query['select'][] = 's.*';
  // If we have the exact fields, make sure we match only the rows with this number of conditions
  if ($exact_fields && isset($field_conditions)) {
    $main_conditions += array('conditions' => count($field_conditions));
  }
  // Query for the notifications table
  $result = notifications_query_subscriptions($main_conditions, $field_conditions, $query);

  // Build list with results, we may need to index by a different field
  if ($key == 'sid') {
    $subscriptions = $result;
  }
  else {
    $subscriptions = array();
    foreach ($result as $subs) {
      if ($key == 'value' || $key == 'intval') {
        $field = array_shift($subs->get_fields());
        $subscriptions[$field->value] = $subs;
      }
      else {
        $subscriptions[$subs->$key] = $subs;
      }      
    }
  }

  return $subscriptions;
}

/**
 * Get active subscriptions for a given user to some object
 * 
 * This is a shorthand function to quickly check some types of subscriptions.
 * 
 * @param $account
 *   User id or user account object to check subscriptions for
 * @param $type
 *   Object type
 * @param $field
 *   Field type for the subscription. I.e. for a node it will be nid
 * @param $value
 *   Field value to check subscriptions to. I.e. $node
 * 
 * @return
 *   Array of subscriptions for this user and object indexed by sid 
 */
function notifications_user_get_subscriptions($account, $type, $object, $refresh = FALSE) {
  $subscriptions = &drupal_static(__FUNCTION__);
  $uid = messaging_user_uid($account);
  // No subscriptions for anonymous users
  if (!$uid) return array();
  
  // Check the object exists, is loaded and we've got a key field
  $object = notifications_object_load($type, $object);
  $key = notifications_object_type($type, 'key_field');
  if (!$object || !$key || empty($object->$key)) return array();
  
  // Now we've got the key fields for caching, try the cache or find subscriptions
  if ($refresh || !isset($subscriptions[$uid][$type][$object->$key])) {
    // Collect subscription types for this object and this account
    // Get allowed subscription options for this account to this object
    $user_subscriptions = array();
    $subscribe_options = notifications_object_subscribe_options($type, $object, messaging_user_object($account));
    foreach ($subscribe_options as $template) {
      $type_subscriptions = notifications_get_subscriptions(
        array('uid' => $uid, 'type' => $template['type'], 'status' => Notifications_Subscription::STATUS_ACTIVE), // Main conditions
        $template['fields'], // Field conditions
        TRUE // Match exactly these condition fields, no less, no more
      );
      if ($type_subscriptions) {
        $user_subscriptions += $type_subscriptions; //array_merge($user_subscriptions, $type_subscriptions);
      }
    }
    $subscriptions[$uid][$type][$object->$key] = $user_subscriptions;
  }
  return $subscriptions[$uid][$type][$object->$key];
}
