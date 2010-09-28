<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Field for notifications objects
 */
class Notifications_Field {
  // Field type
  public $type;
  // Value
  public $value;
  // Index for subscription
  public $index;
  // Object name
  public $name;
  // Object type
  public $object_type = 'object';
  // Linked object
  protected $object;
  // Data type
  protected $data_type = 'int';
  // Subscription this field belongs to
  protected $subscription;
  /**
   * Constructor
   */
  public function __construct($value = NULL, $index = NULL) {
    $this->value = $value;
    $this->index = $index;
  }
  /**
   * Build field instance
   */
  public static function build_type($type, $value, $index = NULL) {
    if ($class = notifications_field($type, 'class')) {
      return new $class($type, $value, $index);
    }
    else {
      $field = new Notifications_Field($value, $index);
      $field->type = $type;
      return $field;
    }
  }
  /**
   * Build from db object
   */
  public static function build_object($object) {
    return self::build_type($object->type, $object->value, $object->index) { 
  }
  /**
   * Get field value name, plain text
   */
  function getName() {
    return ($name = $this->name()) ? check_plain($this->name()) : t('unknown');
  }
  /**
   * Get title for object
   */
  function getTitle() {
    return t('Field');
  }
  /**
   * Get description
   */
  function getDescription() {
    
  }
  /**
   * Get link
   */
  function getLink($options = array()) {
    if ($path = $this->get_path() && $name = $this->getName()) {
      return l($name, $path, $options);
    }
    else {
      return $this->getName();
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
   * Get query conditions for a given object
   * @param $object
   */
  function get_object_condition($object) {
    
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
   * Get related Notifications object
   */
  function get_object() {
    if (!isset($this->object)) {
      $this->object = Notifications_Object::build($this->object_type, $this->value);
    }
    return $this->object;
  }

  /**
   * Get unique index for this field
   */
  function index() {
    return $this->object_type . ':' . $this->type . ':' . (isset($this->value) ? $this->value : '');
  }
  /**
   * Get unfiltered name for this field value
   */
  function name() {
    if (!isset($this->name)) {
      $this->name = $this->get_object()->value() ? $this->get_object()->name() : '';
    }
  }
}

/**
 * Field with autocomplete values
 */
class Notifications_Field_Autocomplete extends Notifications_Field {
  /**
   * Format value for autocomplete
   */
  function autocomplete_value() {
    return $this->getTitle();
  }
  /**
   * Parse value from autocomplete
   */
  public abstract function autocomplete_parse($string);
  /**
   * Get autocomplete path
   */
  public abstract function autocomplete_path();
}

/**
 * Field with selectable values
 */
class Notifications_Field_Select extends Notifications_Field {
  
}
/**
 * Node fields
 */
class Notifications_Node_Field extends Notifications_Field_Autocomplete {
  public $type = 'node';
  public $object_type = 'node';
  /**
   * Get field name
   */
  function getTitle() {
    return t('Node');
  }
  /**
   * Get object name as plain string
   */
  function getName() {
    if ($node = $this->get_object()->value()) {
    return $html ? l($node->title, "node/$nid") : check_plain($node->title);
  }
  else {
    return t('Not found');
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
    return notifications_node_nid2title()
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
  function autocomplete_parse($string, $field = NULL) {
    return notifications_node_title2nid($string, $field)
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
  public $type = 'user';
  public $object_type = 'user';
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
    return notifications_user_name2uid($string, $field)
  }

  function autocomplete_value() {
    return notifications_user_name_callback();
  }

  function format_name($format) {
    return messaging_user_format_name();
  }
}

/**
 * Reverse mapping from node title to nid
 * 
 * We also handle autocomplete values (title [nid:x]) and validate the form
 */
function notifications_node_title2nid($name, $field = NULL, $types = array()) {
  if (!empty($name)) {
    preg_match('/^(?:\s*|(.*) )?\[\s*nid\s*:\s*(\d+)\s*\]$/', $name, $matches);
    if (!empty($matches)) {
      // Explicit [nid:n].
      list(, $title, $nid) = $matches;
      if (!empty($title) && ($node = node_load($nid)) && $title != $node->title) {
        if ($field) {
          form_set_error($field, t('Node title mismatch. Please check your selection.'));
        }
        $nid = NULL;
      }
    }
    else {
      // No explicit nid.
      $reference = _notifications_node_references($name, 'equals', $types, 1);
      if (!empty($reference)) {
        $nid = key($reference);
      }
      elseif ($field) {
        form_set_error($field, t('Found no valid post with that title: %title', array('%title' => $name)));
      }
    }
  }
  return !empty($nid) ? $nid : NULL;  
}

/**
 * Find node title matches.
 * 
 * Some code from CCK's nodereference.module
 */
function _notifications_node_references($string, $match = 'contains', $types = array(), $limit = 10) {
  $match_operators = array(
    'contains' => "LIKE '%%%s%%'",
    'equals' => "= '%s'",
    'starts_with' => "LIKE '%s%%'",
  );
  if ($types) {
    $where[] = 'n.type IN (' . db_placeholders($types, 'char') . ') ';
    $args = $types;
  }
  $where[] = 'n.title '. (isset($match_operators[$match]) ? $match_operators[$match] : $match_operators['contains']);
  $args[] = $string;
  $sql = db_rewrite_sql('SELECT n.nid, n.title, n.type FROM {node} n WHERE ' . implode(' AND ', $where) . ' ORDER BY n.title, n.type');
  $result = db_query_range($sql, $args, 0, $limit) ;
  $references = array();
  while ($node = db_fetch_object($result)) {
    $references[$node->nid] = array(
      'title' => $node->title,
      'rendered' => check_plain($node->title),
    );
  }
  return $references; 
}

/**
 * Get full info array for field type that has the property.
 * 
 * It will return the info array for the field it it has the property or the info for the object type
 * 
 * @param $type
 *   Field type
 * @param $property
 *   Property we are looking for
 *   
 * @return array()
 *   Info array from field or from object type
 */
function notifications_field_get_info($type, $property) {
  if ($info = notifications_field_type($type)) {
    if (isset($info[$property])) {
      return $info;
    }
    elseif (!empty($info['object_type']) && notifications_object_type($info['object_type'], $property)) {
      return notifications_object_type($info['object_type']);
    }
  }
}

/**
 * Format field type name
 */
function notifications_field_format_name($type) {
  $name = notifications_field_type($type, 'name');
  return $name ? $name : t('Unknown');
}

/**
 * Format field value
 * 
 * @param $type
 *   Field type
 * @param $value
 *   Field value
 * @param $html
 *   Whether to format the field as HTML (if FALSE will return plaintext format)
 * @param $subscription
 *   Subscription instance or template for which we want to format this field
 */
function notifications_field_format_value($type, $value, $html = TRUE, $subscription = NULL) {
  if ($format_info = notifications_field_get_info($type, 'format callback')) {
    $format_value = _notifications_info_callback($format_info, 'format callback', array($value, $html));
  }
  elseif ($options = notifications_field_subscription_options($type)) {
    // If not we try options callback, we can get the name from the array of options
    $format_value = isset($options[$value]) ? $options[$value] : t('Not available');
  }

  // If nothing got, we return the value
  if (!isset($format_value)) {
    $format_value = check_plain($value);
  }
  return $format_value;
}

/**
 * Collect submitted fields and parse new values
 */
function notifications_field_parse_submitted(&$form_state, $element_name = 'fields') {
  $fields = array();
  if (!empty($form_state['values'][$element_name]['type'])) {
    $field_values = &$form_state['values'][$element_name];
    foreach ($field_values['type'] as $key => $type) { 
      // If marked for deletion we just keep it there, don't return field
      if (empty($field_values['delete'][$key])) {
        // First collect all field values from the form   
        $field = array('type' => $type, 'value' => $field_values['value'][$key], 'edit' => $field_values['edit'][$key]);  
        // Complete field edit value, depending on field definition.
        if (empty($field_values['parsed'][$key])) {
          $value = notifications_field_real_value($type, $field['edit']);
          if (isset($value)) {
            $field['value'] = $value;
            $field_values['value'][$key] = $value;
            $field['parsed'] = TRUE;
            $field_values['parsed'][$key] = TRUE;
          }
          // Otherwise we let the field keep its value
        }
        // Add field to the list and mark as formatted so we can use this value for the form
        $fields[] = $field;
      }
    }
    
  }
  return $fields;
}

/**
 * Validate submitted field values and set the new ones as valid array of values
 */
function notifications_field_validate_submitted(&$form_state, $element_name = 'fields', $require_one = TRUE, $require_all = TRUE) {
  $checked_values = array();
  if ($field_values = notifications_field_parse_submitted($form_state, $element_name)) {
    foreach ($field_values as $key => $field) {
      $string_id = "$element_name][edit][$key";
      // We validate the field, type included
      if (notifications_field_valid_value($field['edit'])) {
        if (empty($field['parsed']) || !notifications_field_valid_value($field['value'], $field['type'])) {
          form_set_error($string_id, t('The value for this field is not valid.'));
          continue;
        }
      }
      elseif ($require_all) {
        form_set_error($string_id, t('You must set a value for this field.'));
        continue;
      }
      $checked_values[] = array('type' => $field['type'], 'value' => $field['value']);
    }
  }
  elseif ($require_one) {
    form_set_error(NULL, t('You must set at least one field for this subscription type.'));
  }
  return $checked_values;
}

/**
 * Convert field value from submission into its real value
 */
function notifications_field_real_value($type, $value) {  
  if (!notifications_field_valid_value($value)) {
    return NULL;
  }
  elseif ($info = notifications_field_get_info($type, 'value callback')) {
    // We have a value callback for field or object so use it
    return _notifications_info_callback($info, 'value callback', array($value));
  }
  else {
    // As we have nothing better, return the value itself
    return $value;
  }
}

/**
 * Get type information for field. For now its just subscription fields
 */
function notifications_field_type($type = NULL, $property = NULL) {
  return notifications_field_type($type, $property);
}

/**
 * Get options for fields with options callback, may depend on subscription type
 * 
 * - First tries 'subscription_type options callback'
 * - If not found try generic 'options callback'
 */
function notifications_field_subscription_options($type, $subscription = NULL) {
  // First try specific options for this subscription type if any
  if ($subscription && ($info = notifications_field_get_info($type, "$subscription->type options callback"))) {
    return _notifications_info_callback($info, "$subscription->type options callback", array($subscription));
  }
  elseif ($info = notifications_field_get_info($type, 'options callback')) {
    return _notifications_info_callback($info, 'options callback', array($subscription));
  }
}

/**
 * Check if the field has a valid value
 */
function notifications_field_valid_value($value, $type = NULL) {
  // A numeric value of zero is possible too, that's why the is_numeric()
  if (!is_numeric($value) && empty($value)) {
    // The field has no value at all, no go
    return FALSE;
  }
  elseif ($type) {
    // We want aditional field type validation
    switch (notifications_field_type($type, 'type')) {
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
 * Build a form element to edit a field
 * 
 * @param $type
 *   Field type
 * @param $value
 *   Field value
 * @param $subscription
 *   Subscription template or instance we are adding this field to
 */
function notifications_field_form_element($type, $value, $subscription = NULL, $title = FALSE, $required = FALSE, $size = 40) {
  $subscription_type = $subscription ? $subscription->type : NULL;
  $field_info = notifications_field_type($type);
  $object_info = isset($field_info['object_type']) ? notifications_object_type($field_info['object_type']) : array();
  $merged_info = $field_info + $object_info;

  if (isset($merged_info['options callback'])) {
    $element['#type'] = 'select';
    $element['#options'] = notifications_field_subscription_options($type, $subscription);
  }
  elseif (!empty($merged_info['autocomplete path'])) {
    $element['#type'] = 'textfield';
    $element['#size'] = $size;
    $element['#autocomplete_path'] = $merged_info['autocomplete path'];
    if ($value) {     
      if (!empty($merged_info['autocomplete callback'])) {
        $value = _notifications_field_callback($type, 'autocomplete callback', $value, $subscription);
      }
      elseif (!empty($merged_info['format callback']))  {
        $value = _notifications_field_callback($type, 'format callback', $value, FALSE);
      }      
    }
  }
  else {
    $element['#type'] = 'textfield';
    if ($value) {
      $value = check_plain($value);
    }
  }
  if ($value) {
    $element['#default_value'] = $value;
  }
  if ($title) {
    $element['#title'] = notifications_field_format_name($type);
  }
  $element['#required'] = $required;
  return $element;
}

