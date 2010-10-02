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
  
  // Blocked subscriptions, for blocked users
  const STATUS_BLOCKED = 0;
  // Enabled ones, will produce notifications
  const STATUS_ACTIVE = 1;
  // Temporarily disabled ones, maybe user on holidays
  const STATUS_INACTIVE = 2;
  // Disabled because the subscription type is disabled
  const STATUS_DISABLED = -1;

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
   * Build subscription type
   */
  public static function build_type($type) {
    $info = self::type_info($type);
    $class = self::type_info($type, 'class', 'Notifications_Subscription_Type');
    $subscription = new $class($info);
    $subscription->type = $type;
    return $subscription;
  }
  /**
   * Build subscription instance
   */
  public static function build_instance($type) {
    return self::build_type($type)->instance();
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
    $subscriptions = entity_load('notifications_subscription', $sid);
    return reset($subscriptions);
  }
  /**
   * Load multiple from db
   */
  public static function load_multiple($conditions, $fields = array()) {
    // First, fitch sids
    $query = db_select('notifications_subscription', 's')->fields('s', array('sid'));
    foreach ($conditions as $key => $value) {
      $query->condition('s.' . $key, $value);
    }
    foreach ($fields as $field) {
      $alias = $query->join('notifications_subscription_fields');
      $query->condition($field->get_query_condition($alias));
    }
    // Then build entities
    if ($sids = $query->execute()->fetchColumn()) {
      return entity_load('notifications_subscription', $sids);
    }
    else {
      return array();
    }
  }
  /**
   * Get all existing instances form the db
   */
  public function get_instances($params) {
    $params += array('type' => $this->type);
    // Add all the fields that have a value set (type and instance fields)
    $fields = $this->filter_fields($this->get_fields());
    return $this->load_multiple($params, $fields);
  }

  /**
   * Get description
   */
  public abstract function get_description($format = self::FORMAT_HTML);
  /**
   * Get all fields for this subscription/type
   */
  public abstract function get_fields();
  /**
   * Get subscription type object
   */
  public abstract function subscription_type();  

  /**
   * Check whether this is a subscription instance or a subscription type
   */
  public abstract function is_instance();
  /**
   * Check whether this is a stored subscription
   */
  public abstract function is_stored();
  /**
   * Get instance of this subscription type
   */
  public abstract function instance();
  
  /**
   * Get fields that are variable for subscription instance
   */
  public abstract function get_instance_fields();
  /**
   * Get type fields, the ones that have a value set for this type
   */
  function get_type_fields() {
    return $this->filter_fields($this->subscription_type()->get_fields(), TRUE);
  }
  /**
   * Get editable fields, the ones that are not set for type
   */
  function get_editable_fields() {
    return $this->get_instance_fields();
  }
  /**
   * Map field to its right position in this subscription
   */
  function map_field($field) {
    // First we try with instance fields
    foreach ($this->get_instance_fields() as $instance) {
      if ($instance->type == $field->type) {
        return $instance->position;
      }
    }
  }
  /**
   * Get subscription type data
   */
  function get_type($property = NULL) {
    return empty($this->type) ? NULL : notifications_subscription_type($this->type, $property);
  }
  /**
   * Get default options for links
   */
  protected function get_link_options($operation, $options = array()) {
    return $options + array('base_path' => 'notifications/subscription/' . $operation);
  }
  /**
   * Get default link type (susbscribe || unsubscribe)
   */
  protected function get_link_type($type = NULL) {
    return $type ? $type : ($this->is_stored() ? 'unsubscribe' : 'subscribe');
  }
  /**
   * Subscribe links are ok for instances and types
   */
  function get_link($type = NULL, $options = array()) {
    $link = $this->element_link($type, $options);
    return drupal_render($link);
  }

  /**
   * Get default link text for this
   */
  protected function get_link_text($type) {
    return $this->get_title();
  }
  /**
   * Get path for links
   */
  protected function get_path($operation, $options = array()) {
    $options = $this->get_link_options($operation, $options);
    return $options['base_path'] . '/' . $operation;
  }
  /**
   * Get URL for messages (absolute and signed)
   */
  public function get_url($operation, $options = array()) {
    $options += array('absolute' => TRUE, 'signed' => TRUE);
    return $this->get_path($operation, $options);
  }
  
  /**
   * Get link element
   */
  public function element_link($operation = NULL, $options = array()) {
    $type = $this->get_link_type($operation);
    $options = $options + $this->get_link_options($type, $options);
    $path = $this->get_path($type, $options);
    return array(
        '#type' => 'link',
        '#markup' => $this->get_link_text($type),
        '#title' => $this->get_title(),
        '#href'  => $path,
        '#options' => notifications_url_options($path, $options),
        '#description' => $this->get_description(),
      );    
  }

  /**
   * Get subscription type information
   */
  public static function type_info($type = NULL, $property = NULL, $default = NULL) {
    return notifications_subscription_type($type, $property, $default);
  }
  
  /**
   * Filter out fields that have no value set ($isset = FALSE to get the ones that are not set)
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
   * Validate form
   */
  public function form_validate($form, &$form_state) {
    $this->set_properties_from_submission($form, &$form_state);
    return $this->validate_submission($form, &$form_state);
  }
  /**
   * Process form submission
   */
  public function form_submit($form, &$form_state) {
    $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : '';
    if ($op == t('Cancel')) {
      $operation = 'cancel';
    }
    else {
      $operation = $form_state['values']['operation'];
      $this->set_properties_from_submission($form, &$form_state);
    }
    return $this->form_operation($operation, $form, $form_state);
  }
  
  /**
   * Run form operation
   */
  protected function form_operation($operation, $form, &$form_state) {
    switch ($operation) {
      case 'cancel':
        drupal_set_message(t('Operation cancelled.'), 'warning');
        return FALSE;
      case 'confirm':
      case 'add':     
      case 'edit':
        $result = $this->save();
        switch ($result) {
          case SAVED_NEW:
            drupal_set_message(t('Your subscription has been created.'));
            break;
          case SAVED_UPDATED;
            drupal_set_message(t('Your subscription has been updated.'));
            break;
          default:
            drupal_set_message(t('Error saving your subscription.'), 'warning');
        }
        return $result;

      default:
        drupal_set_message('Operation not implemented yet.', 'warning');
        return FALSE;
    }
  }
  /**
   * Get form for this subscription
   */
  public function get_form($operation, $form, $form_state) {
    $form['subscription'] = array('#type' => 'value', '#value' => $this);
    $form['operation'] = array('#type' => 'value', '#value' => $operation);
    $form['subscription_info'] = $this->element_info();
    $form['send_method'] = $this->element_send_method();
    $form['send_interval'] = $this->element_send_interval();
    if ($fields = $this->get_editable_fields()) {
      $form['subscription_fields'] = $operation == 'confirm' ? $this->element_fields_info($fields) : $this->element_fields_edit($fields);
    }
    $form['controls'] = $this->element_operation($operation);
    return $form;
  }
  
  /**
   * Get subscription form info element
   */
  public function element_info() {
    $elements = array(
      '#type' => 'fieldset',
      '#title' => $this->get_title(),
    );
    $elements['name']['#markup'] = $this->get_name();
    $elements['description']['#markup'] = $this->get_description();
    // We may have fixed fields or not
    if ($fields = $this->get_type_fields()) {
      $elements['fields'] = $this->element_fields_info($fields);
    }
    return $elements;
  }
  /**
   * Send method: form element
   */
  public function element_send_method() {
    $element = array();
    $send_methods = notifications_send_methods($this->get_user());
    if (count($send_methods) > 1) {
      $element = array(
        '#title' => t('Send method'),
        '#type' => 'options',
        '#options' => $send_methods,
        '#default_value' => $this->send_method,
      );
    }
    return $element;    
  }
  /**
   * Send method: form element
   */
  public function element_send_interval() {
    $send_intervals = notifications_send_intervals($this->get_user());
    $element = array();
    if (count($send_intervals) > 1) {
      $element = array(
        '#title' => t('Send interval'),
        '#type' => 'options',
        '#options' => $send_intervals,
        '#default_value' => $this->send_interval,
      );
    }
    return $element;    
  }
  /**
   * Get all fields (editable and not editable)
   */
  public function element_fields() {
    $element = array();
    if ($type_fields = $this->get_type_fields()) {
      $elements['info'] = $this->element_fields_info($fields);
    }
    if ($edit_fields = $this->get_editable_fields()) {
      $elements['edit'] = $this->element_fields_info($fields);
    }
    return $element;
  }
  /**
   * Get elements for information fields (non editable)
   */
  public function element_fields_info($fields) {
    $elements = array();
    foreach ($fields as $field) {
      $elements[$field->position] = $field->element_info();
    }
    return $elements;
  }
  /**
   * Get form elements for editable fields
   */
  public function element_fields_edit($fields) {
    $elements = array('#tree' => TRUE);
    foreach ($fields as $field) {
      $elements[$field->position] = $field->element_edit();
    }
    return $elements;
  }
  /**
   * Get operation buttons for form
   */
  public function element_operation($operation) {
    $elements = array('#type' => 'fieldset');
    switch ($operation) {
      case 'add':
      case 'confirm':
        $elements['subscribe'] = array('#type' => 'submit', '#value' => t('Create subscription'));
        break;
      case 'edit':
        $elements['save'] = array('#type' => 'submit', '#value' => t('Save subscription'));
        // no break
      case 'delete':
        $elements['delete'] = array('#type' => 'submit', '#value' => t('Delete subscription'));
        break;
    }
    $elements['cancel'] = array('#type' => 'submit', '#value' => t('Cancel'));
    return $elements;
  }

  /**
   * Build subscription instance from form submission
   */
  public static function build_from_submission($form, &$form_state) {
    // The subscription object should be here, it may be a subscription type
    $subscription = $form_state['values']['subscription'];
    return $subscription->is_instance() ? $subscription : $subscription->get_instance();
  }
  /**
   * Set instance properties from form submission
   */
  protected function set_properties_from_submission($form, &$form_state) {
    // Process regular values and validate
    $this->set_properties_from_values($form_state['values'], TRUE);
    // There may be optional fields to add
    if ($fields = $this->build_fields_from_submission($form, &$form_state)) {
      
    }
    return $this;
  }
  /**
   * Set instance properties from url parameters
   */
  public function set_properties_from_url($query = NULL) {
    $params = isset($query) ? $query : $_GET;
    return $this
      ->set_properties_from_values($params, TRUE)
      ->set_fields_from_values($params);
  }
  /**
   * Set instance properties from array of values
   */
  public function set_properties_from_values($values, $validate = TRUE) {
      // Process some other fields that may be there
    foreach (array('send_method', 'send_interval') as $field) {
      if (isset($values[$field])) {
        $this->set_property($field, $values[$field]);
      }
    }
    return $this; 
  }
  /**
   * Build instance fields form URL parameters
   * 
   * @param $values
   *   Array of field values indexed by field position
   */
  protected function set_fields_from_values($values = array()) {
    $result = TRUE;
    // Set field values, they come in the query string indexed by field position
    foreach ($this->get_editable_fields() as $field) {
      if (isset($values[$field->position])) {
        // Validate (TRUE) and set value
        $result = $result && $field->set_value($values[$field->position], TRUE);
      }
    }
    return $this;
  }
  /**
   * Set property, possibly from form submission
   */
  protected function set_property($name, $value, $validate = TRUE) {
    $this->$name = $value;
  }
  /**
   * Build submitted fields
   */
  protected function build_fields_from_submission($form, &$form_state) {
    $fields = array();
    if (!empty($form_state['values']['subscription_fields'])) {
      $field_values = $form_state['values']['subscription_fields'];
      // In this case we have known fields that are always indexed by position
      foreach ($this->subscription_type()->get_instance_fields() as $field) {
        if (isset($field_values[$field->position])) {
          $build = Notifications_Field::build_from_value($field_values[$field->position], $field->type, $field->position);
          if ($build) {
            $fields[$field->position] = $build;
          }
        }
      }
    }
    return $fields;
  }
  /**
  * Validate form submission
  */
  public static function validate_submission($form, &$form_state) {
    // @todo
  }

  /**
   * Build subscriptions for array of objects and user account.
   * 
   * @return Notifications_Subscription_List
   */
  static function object_subscriptions($objects, $account) {
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
   * Delete multiple subscriptions and clean up related data (pending notifications, fields).
   * 
   * Was notifications_delete_subscriptions ($params, $field_conditions = array(), $limit = FALSE)
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
  public static function delete_multiple($conditions, $field_conditions = array(), $limit = FALSE) {
    $query = db_select('notifications_subscription', 's')->fields('s', array('sid'));
    foreach ($condtions as $field => $value) {
      $query->condition($field, $value);
    }
    // For exact condition fields we add one more main condition.
    if ($limit) {
      $query->condition('conditions', count($field_conditions));
    }
    if ($field_conditions) {
      foreach ($field_conditions as $type => $value) {
        $alias = $query->join('notifications_subscription_fields');
        $query->condition($alias . '.type', $type);
        $query->condition($alias . '.value', $value);
      }
    }
    if ($sids = $query->execute()->fetchCol()) {
      return self::delete_subscription($sids);
    }
  }

  /**
   * Delete subscription and clean up related data, included the static cache
   * 
   * Was: notifications_delte_subscription ($sid)
   * It also removes pending notifications related to that subscription 
   * 
   * @param $sid
   *   Subscription object or sid or array of sids of subscription/s to delete
   */
  public static function delete_subscription($sid) {
    db_delete('notifications_subscription')->condition('sid', $sid)->execute();
    db_delete('notifications_subscription_fields')->condition('sid', $sid)->execute();
    // Delete queued notifications for this subscription
    notifications_queue()->queue_delete(array('sid' => $sid));
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
  public function is_instance() {
    return FALSE;
  }
  public function is_stored() {
    return FALSE;
  }
  /*
   * Get instance, will be the same object
   */
  public function instance() {
    $class = $this->instance;
    return new $class(array('type' => $this->type));
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
    return $this->get_type('title', t('Subscription type'));
  }
  /**
   * Get object name
   */
  public function get_name() {
    return $this->get_type('name', t('Subscription'));
  }
  public function get_description($format = self::FORMAT_HTML) {
    return $this->get_type('description');
  }
  /**
   * Get path for links
   */
  protected function get_path($operation, $options = array()) {
    $options = $this->get_link_options($operation, $options);
    return $options['base_path'] . '/' . $operation . '/' . $this->type;
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
      $value = isset($this->field_values[$index]) ? $this->field_values[$index] : NULL;
      $fields[$index] = Notifications_Field::build_type($type, array('value' => $value, 'position' => $index));
    }
    return $fields;
  }
  /**
   * Get instance fields that don't have a value set for this type
   */
  function get_instance_fields() {
    return $this->filter_fields($this->get_fields(), FALSE);
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
  // This class inherits some properties from Messaging_Object
  public $uid;
  public $language;
  
  // Unique subscription id
  public $sid = 0;
  // Destination id
  public $mdid;
  // Subscription type
  public $type;
  // Event that triggers this subscription
  public $event_type;
  // Number of conditions that must be met
  public $conditions = 0;
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
  /*
   * Get instance, will be the same object
   */
  public function instance() {
    return $this;
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
   * Get default link options
   */
  protected function get_link_options($operation, $options = array()) {
    switch ($operation) {
      case 'subscribe':
      case 'add':
        $options += array(
          'query' => array(),
          'base_path' => 'notifications/subscribe',
          'skip_confirmation' => variable_get('notifications_option_subscribe_links', 0)
        );
        // Add instance fields to the query string indexed by position
        foreach ($this->filter_fields($this->get_fields()) as $field) {
          $options['query'][$field->position] = $field->value;
        }
      case 'unsubscribe':
        return $options + array(
          'base_path' => 'notifications/unsubscribe',
          'skip_confirmation' => variable_get('notifications_option_unsubscribe_links', 0)
        );     
      default:
        return parent::get_link_options($operation, $options);
    }
  }
  /**
   * Get subscribe / unsubscribe text for this
   */
  protected function get_link_text($type, $options = array()) {
    switch ($type) {
      case 'subscribe':
        return t('Subscribe to: @name', array('@name', $this->get_name()));
      case 'unsubscribe':
        return t('Unsubscribe from: @name', array('@name', $this->get_name()));
      default:
        return parent::get_link_text($type);
    }
  }
    /**
   * Get path for links
   */
  protected function get_path($operation, $options = array()) {
    $options = $this->get_link_options($operation, $options);
    switch ($operation) {
      case 'subscribe':
      case 'add':
        return $options['base_path'] . '/' . $this->type;
      case 'unsubscribe':
        return $options['base_path'] . '/' . $this->sid; 
      default:
        return parent::get_path($operation, $options);
    }
  }

  /**
   * Set name for this specific subscription
   */
  function set_name($name) {
    $this->name = $name;
    return $this;
  }
  /**
   * Set user account as the owner of this subscription and take care of defaults for this account.
   * 
   * @param $account
   *   User account or user uid
   */
  function set_user($account) {
    $account = messaging_user_object($account);
    $this->uid = $account->uid;
    if (!isset($this->send_interval)) {
      $this->send_interval = notifications_user_setting('send_interval', $account, 0);
    }
    if (!isset($this->send_method)) {
      $this->send_method = notifications_user_setting('send_method', $account);
    }
    if (empty($this->language)) {
      $this->language = user_preferred_language($account)->language; 
    }
    return $this;
  }

  /**
   * Load condition fields from db
   */
  function load_fields() {
    // Make sure fields are set
    $this->set_fields();
    if (!empty($this->sid)) {
      foreach (Notifications_Field::load_multiple(array('sid' => $this->sid)) as $field) {
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
      db_query("DELETE FROM {notifications_subscription_fields} WHERE sid = %d", $this->sid);
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
   * Check destination (user, method, address, etc...)
   */
  function check_destination() {
    if ($this->get_user()) {
      return !empty($this->send_method);
    }
    else {
      return !empty($this->send_method) && !empty($this->mdid);
    }
  }
  /**
   * Check permission for user account
   */
  function check_access($account = NULL) {
    $account = $account ? $account : $this->get_account();
    // @todo check access to all subscription fields
    
  }
  /**
   * Create destination for this subscription
   */
  function create_destination($method = NULL, $address = NULL) {
    $account = $this->get_user();
    if (Messaging_Destination::validate_method($method, $address, $account)) {
      $destination = Messaging_Destination::build_method($method, $address, $account);
      $this->set_destination($destination);
    }
  }


  /**
   * Save to db
   */
  function save() {
    $update = $this->is_stored();
    $result = drupal_write_record('notifications_subscription', $this, $update ? 'sid' : array());
    return $result && $this->save_fields($update);
  }

  /**
   * Save condition fields to db
   * 
   * @param $update
   *   Whether this is an old subscription being created
   */
  function save_fields($update = FALSE) {
    $result = TRUE;
    if (isset($this->fields)) {
      if ($update) {
        db_query('DELETE FROM {notifications_subscription_fields} WHERE sid = :sid', array(':sid' => $this->sid));
      }
      foreach ($this->get_fields() as $field) {
        $field->set_subscription($this);
        $result = $result && $field->save();
      }
    }
    return $result;
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
   * Add field from type, value
   */
  function add_field($type, $value) {
    $field = Notifications_Field::build($type, $value);
    return $this->set_field($field);
  }
  /**
   * Set a field, we may need to find out the position
   */
  function set_field($field, $overwrite = TRUE) {
    $field->set_subscription($this);
    if (!isset($field->position)) {
      $field->position = $this->map_field($field);
    }
    $this->fields[$field->position] = $field;
    $this->conditions = count($this->fields);
    return $this;
  }
  /**
   * Get a field for a given position
   */
  function get_field($position) {
    $this->set_fields();
    return isset($this->fields[$position]) ? $this->fields[$position] : NULL;
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
    $this->set_fields();
    $editable = array();
    foreach ($this->subscription_type()->get_instance_fields() as $type_field) {
      // Get current instance field if available, new one (from type) if not
       if ($field = $this->get_field($type_field->position)) {
         $editable[$field->position] = $field;
       }
       else {
         $this->set_field($type_field);
         $editable[$type_field->position] = $type_field;
       }
    }
    return $editable;
  }

  /**
   * Get editable fields, usually same as instance fields
   */
  function get_editable_fields() {
    return $this->get_instance_fields();
  }
  
  /**
   * Get user account
   */
  function get_user() {
    return isset($this->uid) ? user_load($this->uid) : NULL;
  }
  /**
   * Check all fields are there and optinally that they have a value
   * 
   * @todo check values?
   */
  function check_fields($check_values = TRUE) {
    $this->set_fields();
    $fields = $this->get_fields();
    return count($fields) === count($this->filter_fields($fields));
  }
  /**
   * Set field values, all at a time from the ones coming from the subscription type.
   */
  function set_fields($fields = NULL) {
    if (isset($fields)) {
      $this->fields = array();
      foreach ($fields as $field) {
        $this->set_field($field);
      }
    }
    elseif (!isset($this->fields)) {
      $this->set_fields($this->subscription_type()->get_fields());
    }
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
    return $this;
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
   * Get instance of this one for certain conditions
   * 
   * If we got multiple ones, return just the first one
   * 
   * @param $params
   *   Parameters to add to the subscription type to get an instance of itself
   */
  function get_instance($params = array(), $return_self = FALSE) {
    $params += array('type' => $this->type);
    if ($instances = $this->get_instances($params)) {
      return reset($instances);
    }
    else {
      return $return_self ? $this : FALSE;
    }
  }

  /**
   * Load subscription objects, from fields
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
  public function get_description($format = self::FORMAT_HTML) {
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
    return $field->format($format);

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

}

