<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Common base for subscription type and subscription instance
 */
abstract class Notifications_Subscription extends Notifications_Entity {
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
  
  // Subscription type
  protected $type;
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
  /**
   * Build from db object or template
   */
  public static function build_object($object) {
    $class = self::type_info($object->type, 'class', 'Notifications_Subscription_Instance');
    return new $class($object);
  }
  /**
   * Get all fields for this subscription/type
   */
  public abstract function get_fields();
  /**
   * Get subscription type object
   * 
   * @return Notifications_Subscription_Type
   */
  public abstract function subscription_type();  
  /**
   * Check whether this is a subscription type
   */
  public function is_type() {
    return FALSE;
  }
  /**
   * Check whether this is a subscription instance
   */
  public function is_instance() {
    return FALSE;
  }

  /**
   * Get subscription type information
   */
  public static function type_info($type = NULL, $property = NULL, $default = NULL) {
    return notifications_subscription_type($type, $property, $default);
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
   * Filter out fields that have no value set ($isset = FALSE to get the ones that are set)
   */
  public static function filter_fields($fields, $isset = TRUE) {
    $result = array();
    foreach ($fields as $key => $field) {
      if ($isset && isset($field->value) || !$isset && !isset($field->value)) {
        $result[$key] = $field;
      }
    }
    return $result;    
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
  

  /**
   * Get objects to which we can subscribe on current page.
   * 
   * Was: notifications_object_page_objects()
   */
  static function page_objects() {
    $objects = &drupal_static(__FUNCTION__);
    if (!isset($objects)) {
      $objects = module_invoke_all('notifications_subscription', 'page objects');
    }
    return $objects;
  }

  /**
   * Build subscriptions for current user to an array of objects.
   * 
   * Was notifications_object_page_subscriptions($objects)
   * 
   * @return Notifications_Subscription_List
   */
  static function page_subscriptions($objects, $account = NULL) {
    $account = $account ? $account : $GLOBALS['user'];
    $subscriptions = new Notifications_Subscription_List();
    foreach ($objects as $object) {
      if ($more = $object->user_subscriptions($account)) {
        $subscriptions->add($more);
      }
    }
    return $subscriptions;
  }

  /**
   * Build a form element for a field
   * 
   * A subscription type can override its form elements
   */
  function field_form_element($field, $title = FALSE, $required = FALSE, $size = 40) {
    return $field->form_element($title, $required, $size);
  }

  /**
   * Run module_invoke_all('notifications_subscription') with this object
   */
  protected function invoke_all($op, $param = NULL) {
    return module_invoke_all('notifications_subscription', $op, $this, $param);    
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
  // Array of field types
  public $field_types = array();
  // Array of field values
  public $field_values = array();
  // Array of built field objects
  public $fields = array();
  
  /**
   * This is a subscription type
   */
  public function is_type() {
    return TRUE;
  }
  /**
   * Get subscription type object
   * 
   * @return Notifications_Subscription_Type
   */
  public function subscription_type() {
    return $this;
  }
  /**
   * Get object title
   */
  public function get_title() {
    return t('Subscription type');
  }
  /**
   * Get object name
   */
  public function get_name() {
    return t('Subscription');
  }

  /**
   * User access function
   */
  public function user_access($account, $op = 'view') {
    $access = $this->get_info('access');
    // Access may be true/false or an array of permissions
    if ($access && is_array($access)) {
      foreach ($access as $perm) {
        if (!user_access($perm, $account)) {
          return FALSE;
        }
      }
      return TRUE;
    }
    else {
      return (boolean)$access;
    }
  }
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
  public function build_instance($object = NULL) {
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
   * 
   * These are instance fields so we don't store them.
   */
  function get_fields() {
    $fields = array();
    foreach ($this->field_types as $index => $type) {
      $value = issset($this->field_values[$index]) ? $this->field_values[$index] : NULL;
      $fields[$index] = Notifications_Field::build($type, $value, $index);
    }
    return $fields;
  }
  /**
   * Get instance fields that don't have a value set for this type
   */
  function get_instance_fields() {
    return $this->filter_fields($this->get_fields());
  }
  /**
   * Get type fields, the ones that have a value set for this type
   */
  function get_type_fields() {
    return $this->filter_fields($this->get_fields(), TRUE);
  }
  /**
   * Get editable fields, usually same as instance fields
   */
  function get_editable_fields() {
    return $this->get_instance_fields();
  }

  /**
   * Set fields
   */
  function set_fields($fields) {
    if ($fields) {
      foreach ($fields as $field) {
        $field->subscription = $this;
        $this->fields[$field->index] = $field;
      }
    }
  }
  
  /**
   * Get event conditions OR'd
   * 
   * These will look like
   * 
   * s.type = 'node' AND (field_condition1 OR field_condition2)
   * s.type = 'term' AND (field_condition1)
   */
  function get_event_conditions($event, $add) {
    foreach ($event->get_objects() as $object) {
      $condition = $this->get_object_conditions($object, db_or());
      if ($condition && $condition->count()) {
        $add->condition($condition);
      }
    }
    // Finally, add subscription type
    return db_and()
      ->condition('type', $this->type)
      ->condition($add);
  }

  /**
   * Get OR'd field conditions for this subscription type and object
   * 
   * Conditions will look like
   *   f.type = 'nid' AND f.value = 100
   *   f.type = 'term' AND f.value IN (1, 2, 3, 4)
   **/
  function get_object_conditions($object, $add) {
    foreach ($this->get_unique_fields() as $field) {
      if ($condition = $field->get_object_condition($object)) {
        $add->condition($condition);
      }
    }
    return $add;
  }
  /**
   * Get unique field per type.
   * 
   * A subscription may have more than one field of the same type
   */
  function get_unique_fields() {
    $fields = array();
    foreach ($this->get_fields() as $field) {
      $fields[$field->type] = $field;
    }
    return $fields;
  }
  /**
   * Get a subform to edit field elements. 
   * 
   * Defaults to the subscription type method with this instance fields.
   */
  function fields_edit_fieldset($fields = NULL) {
    $fields = isset($fields) ? $fields : $this->get_fields();
    $elements['fields'] = array(
      '#type' => 'fieldset',
      '#title' => t('Subscription fields'),
    ) + $this->fields_edit_elements($fields);
    return $elements;
  }
  
  /**
   * Get elements to edit fields
   */
  function fields_edit_elements($fields = NULL) {
    $fields = isset($fields) ? $fields : $this->get_fields();
    $elements = array();
    foreach ($fields as $index => $field) {
      $elements[$index] = $field->form_edit_element();
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
   * Check whether this is a subscription instance
   */
  public function is_instance() {
    return TRUE;
  }
  /**
   * Check whether this is a stored instance
   */
  public function is_stored() {
    return !empty($this->sid);
  }
  /**
   * User access function
   */
  public function user_access($account, $op = 'view') {
    // First check access to subscription type
    return $this->subscription_type()->user_access($account, $op);
    
  }
  /**
   * Get subscription type object
   * 
   * @return Notifications_Subscription_Type
   */
  public function subscription_type() {
    return notifications_subscription($this->type);
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
  function set_field($field) {
    $field->subscription = $this;
    $this->fields[$field->position] = $field;
  }
  /**
   * Get fields as array of field objects
   */
  function get_fields() {
    if (!isset($this->fields)) {
      if ($this->is_stored()) {
        $this->load_fields();
      }
      else {
        $this->set_fields();
      }
    }
    return $this->fields;
  }
  /**
   * Get instance fields that don't have a value set for this type
   */
  function get_instance_fields() {
    return $this->subscription_type()->get_instance_fields();
  }
  /**
   * Get type fields, the ones that have a value set for this type
   */
  function get_type_fields() {
    return $this->subscription_type()->get_type_fields();
  }
  /**
   * Get editable fields, usually same as instance fields
   */
  function get_editable_fields() {
    return $this->get_instance_fields();
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
    return $this->subscription_type()->get_type_fields();
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
   * Get a subform to edit field elements. 
   * 
   * Defaults to the subscription type method with this instance fields.
   */
  function fields_edit_fieldset($fields = NULL) {
    $fields = isset($fields) ? $fields : $this->get_fields();
    return $this->subscription_type()->fields_edit_fieldset($fields);
  }
  
  /**
   * Get elements to edit fields
   */
  function fields_edit_elements($fields = NULL) {
    $fields = isset($fields) ? $fields : $this->get_fields();
    return $this->subcription_type()->fields_edit_elements($fields);   
  }

  /**
   * Get element to edit field
   */
  function field_edit_element($field) {
    return $this->subscription_type()->field_edit_element($field);
  }
  /**
   * Display a form field for a notifications_field
   *   
   * Defaults to the subscription type display
   * 
   * @param $type
   *   Field type
   * @param $value
   *   Field value
   * @param $subscription
   *   Subscription template or instance we are adding this field to
   */
  function field_form_element($field, $title = FALSE, $required = FALSE, $size = 40) {
    return $this->subscription_type()->field_form_element($field, $title, $required);
  }
  /**
   * Display a form field for a notifications_field
   * 
   * 
   */
  public function field_element($field, $title = FALSE, $required = FALSE ) {
    return $this->subscription_type()->field_element($field, $title, $required);
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
}

/**
 * List of subscriptions or subscription types
 */
class Notifications_Subscription_List {
  public $subscriptions = array();
  protected $instances = array();
  // Array indexed by subscription type
  protected $types = array();
  // Parameters common to all instances
  protected $instance_params = array();
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
    elseif (is_object($items)) {
      if (is_a($items, 'Notifications_Subscription_List')) {
        $this->add($items->get_subscriptions());
      }
      elseif (is_a($items, 'Notifications_Subscription')) {
        $this->add_subscription($items);
      }
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
   * Get array of subscription / subscription types
   */
  function get_subscriptions() {
    return $this->subscriptions;
  }
  /**
   * Get all of the subscriptions that are actual instances
   */
  function get_instances() {
    $list = array();
    foreach ($this->subscriptions as $subscription) {
      if ($subscription->is_instance()) {
        $list[] = $subscription;
      }
    }
    return $list;
  }
  
  /**
   * Set a property to all of the subscription instances
   */
  function set_all($name, $value) {
    $this->instance_params[$name] = $value;
    foreach ($this->get_instances() as $subscription) {
      $subscription->$name = $value;
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
