<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Base class for Notifications fields
 */
abstract class Notifications_Field {
  // Subscription id
  public $sid;
  // Field type
  public $type;
  // Value
  public $value;
  // Index for subscription
  public $position;
  // Object name
  public $name;
  // Object type
  public $object_type = 'object';
  // Linked object
  protected $object;
  // Data type
  protected $data_type = 'int';
  // Subscription object this field belongs to
  protected $subscription;

  /**
   * Constructor
   */
  public function __construct($template = array()) {
    foreach ((array)$template as $field => $value) {
      $this->$field = $value
    }
  }
  /**
   * Build field instance
   */
  public static function build_type($type, $template = NULL) {
    if ($class = notifications_field_type($type, 'class')) {
      return new $class($template);
    }
    else {
      // Unknown field type, let's build something
      $field = new Notifications_Field_Default($template);
      $field->type = $type;
      return $field;
    }
  }
  /**
   * Build from db object
   */
  public static function build_object($object) {
    return self::build_type($object->type, $object);
  }
  /**
   * Get field value name, plain text
   */
  function get_name() {
    if (isset($this->name)) {
      return check_plain($this->name);
    }
    else if ($object = $this->get_object()) {
      return $this->name = $object->get_name();
    }
  }
  /**
   * Get title for field
   */
  public abstract function get_title();
  /**
   * Get description
   */
  public abstract function get_description();


  /**
   * Get link if this field is linked to an object
   */
  function get_link($options = array()) {
    if ($path = $this->get_path() && $name = $this->get_name()) {
      return l($name, $path, $options);
    }
  }
  /**
   * Get system path
   */
  function get_path() {
    return '';
  }
  /**
   * Get related Notifications object
   */
  function get_object() {
    if (!isset($this->object)) {
      $this->object = Notifications_Object::build($this->object_type, $this->value);
    }
    return $this->object;
  }

  /**
   * Get query condition for current value
   */
  function get_query_condition() {
    if (isset($value)) {
      return $this->get_value_condition($value);
    }
  }
  /**
   * Get query condition for a given value
   */
  function get_value_condition($value) {
    $and = db_and();
    $and->condition('type', $this->type);
    if ($this->data_type == 'int') {
      $and->condition('intval', (int)$value);
    }
    else {
      $and->condition('value', $value);
    }
    return $and;
  }
  /**
   * Check user access
   */
  function user_access($account) {
    return $this->get_object()->user_access($account);
  }

  /**
   * Get unique index for this field
   */
  function index() {
    return $this->object_type . ':' . $this->type . ':' . (isset($this->value) ? $this->value : '');
  }

  /**
   * Check if the field has a valid value
   */
  function valid_value() {
    return $this->validate_value($this->value, $this->data_type);
  }

  /**
   * Check if the value is valid for this field has a valid value
   * 
   * Was: notifications_field_valid_value($value, $type = NULL)
   */
  static function validate_value($value, $data_type = NULL) {
    // A numeric value of zero is possible too, that's why the is_numeric()
    if (!is_numeric($value) && empty($value)) {
      // The field has no value at all, no go
      return FALSE;
    }
    elseif ($field_type) {
      // We want aditional field type validation
      switch ($data_type) {
        case 'int':
          // @todo Better integer validation, is_int not working for strings
          return is_numeric($value);
        case 'float':
          return is_numeric($value);
        case 'string':
        default:
          return is_string($value);
      }
    }
    else {
      return TRUE;
    }
  }
 
  /**
   * Build a form element to edit this field
   * 
   * Was: notifications_field_form_element($type, $value, $subscription = NULL, $title = FALSE, $required = FALSE, $size = 40)
   */
  function form_edit_element($title = TRUE, $required = FALSE, $size = 40) {
    $element = array(
      '#type' => 'textfield',
      '#default_value' => isset($this->value) ? $this->value : NULL,
      '#required' => $required,
      '#size' => 40,
    );
    if ($title) {
      $element['#title'] = $this->get_title();
    }
    return $element;
  }
  /**
   * PHP Magic. Regurn object properties to be serialized
   */
  public function __sleep() {
    return array('type', 'value', 'index', 'name');
  }
}

/**
 * Default field when we don't have information
 */
class Notifications_Field_Default {
  /**
   * Get title for field
   */
  function get_title() {
    return t('Field');
  }
  /**
   * Get description
   */
  function get_description() {
    return t('Notifications field');
  }  
}
/**
 * Field with autocomplete values
 */
abstract class Notifications_Field_Autocomplete extends Notifications_Field {
  /**
   * Format value for autocomplete
   */
  function autocomplete_value() {
    return $this->get_title();
  }
  /**
   * Parse value from autocomplete
   */
  public abstract function autocomplete_parse($string);
  /**
   * Get autocomplete path
   */
  public abstract function autocomplete_path();

  /**
   * Build a form element to edit this field
   */
  function form_edit_element($title = TRUE, $required = FALSE, $size = 40) {
    $element = parent::form_edit_element($title, $required, $size);
    $element['#default_value'] = $this->autocomplete_value();
    $element['#autocomplete_path'] = $this->autocomplete_path();
    return $element;
  }
    
}

/**
 * Field with selectable values
 */
abstract class Notifications_Field_Select extends Notifications_Field {
  public abstract function select_options();
  /**
   * Build a form element to edit this field
   */
  function form_edit_element($title = TRUE, $required = FALSE, $size = 40) {
    $options = $this->select_options();
    // If not required or not current value, add an empty value at the beginning
    if (!$required || !isset($this->value)) {
      $options = array('' => '') + $options;
    }
    $element = array(
      '#type' => 'select',
      '#size' => $size,
      '#options' => $options,
      '#default_value' => isset($this->value) ? $this->value : '',
      '#required' => FALSE
    );
    return $element;
  }
}

/**
 * Node fields
 */
class Notifications_Node_Field extends Notifications_Field_Autocomplete {
  public $type = 'node:nid';
  public $object_type = 'node';
  /**
   * Get field name
   */
  function get_title() {
    return t('Node');
  }
  /**
   * Get description
   */
  function get_description() {
    return t('Node');
  } 
  /**
   * Get object name as plain string
   */
  function get_name() {
    if ($node = $this->get_object()->value()) {
      return $html ? l($node->title, "node/$nid") : check_plain($node->title);
    }
    else {
      return t('Not found');
    }
  }
  
  /**
   * Get system path
   */
  function get_path() {
    return isset($this->value) ? 'node/' . $this->value : ''; 
  }
  /**
   * Format object name
   */
  function format_name($format = NOTIFICATIONS_FORMAT_HTML) {
    return notifications_node_nid2title();
  }
  /**
   * Get autocomplete path
   */
  function autocomplete_path() {
    return 'notifications/autocomplete/node/title';
  }
  /**
   * Parse value from autocomplete
   */
  function autocomplete_parse($string) {
    return notifications_node_title2nid($string, $field);
  }
  /**
   * Format value for autocomplete
   */
  function autocomplete_value() {
    $node = $this->get_object();
    return $node ? check_plain($node->title) . ' [nid:' . $node->nid .']' : t('Not found');
  }
  // Access callback
  function user_access($account) {
    return ($node = $this->object()) ? node_access('view', $node, $account) : FALSE;
  }
  /**
   * Get query condition for nodes
   */
  function get_object_condition($object) {
    if ($object->type == 'node' && isset($object->value)) {
      return $this->get_value_condition($object->value);
    }
  }
}

/**
 * Drupal user
 */
class Notifications_User_Field extends Notifications_Field_Autocomplete {
  public $type = 'user:uid';
  public $object_type = 'user';
  /**
   * Get title for field
   */
  function get_title() {
    return t('User');
  }
  /**
   * Get description
   */
  function get_description() {
    return t('User name');
  } 
   /**
   * Get autocomplete path
   */
  function autocomplete_path() {
    return 'user/autocomplete';
  }

  /**
   * Parse value from autocomplete
   */
  function autocomplete_parse($string, $field = NULL) {
    return notifications_user_name2uid($string, $field);
  }

  function autocomplete_value() {
    return notifications_user_name_callback();
  }

  function format_name($format) {
    return messaging_user_format_name($this->value);
  }
}
