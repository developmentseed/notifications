<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * List of subscriptions or subscription types
 */
class Notifications_Subscription_List implements Iterator {
  // The list type will be a unique name identifier, for static caching
  public $type = 'subscriptions';
  public $subscriptions = array();
  // Template for multiple subscriptions of the same type
  public $template;
  // Header for tableselect
  public $header;
  
  protected $instances = array();
  // Array indexed by subscription type
  protected $types = array();
  // Parameters common to all instances
  protected $instance_params = array();
  // Index for Iterator
  private $index = 0;

  /**
   * Construct with list of subscriptions
   */
  public function __construct($type = 'subscriptions') {
    $this->type = $type;
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
   * Load multiple subscriptions into the list
   */
  function load_multiple($sids = array(), $conditions = array()) {
    $this->instance_params = $conditions;
    if ($subs = entity_load('notifications_subscription', $sids, $conditions)) {
      $this->add($subs);
    }
    return $this;
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
   * Build instances for all with given parameters
   * 
   * @todo Handle multiple instances with the same parameters
   */
  function build_instances($params = array()) {
    $this->instance_params = $params;
    foreach ($this->subscriptions as $index => $subscription) {
      if ($instance = $subscription->get_instance($params)) {
        $this->subscriptions[$index] = $instance;
      }
      else {
        $this->subscription[$index] = $subscription->instance();
      }
    }
  }
  /**
   * Set user to all subscriptions
   */
  function set_user($user) {
    $this->instance_params['uid'] = $user->uid;
    foreach ($this->get_instances() as $subscription) {
      $subscription->set_user($user);
    }
    return $this;
  }
  /**
   * Set a property to all of the subscription instances
   */
  function set_all($name, $value) {
    $this->instance_params[$name] = $value;
    foreach ($this->get_instances() as $subscription) {
      $subscription->$name = $value;
    }
    return $this;
  }

  /**
   * Get all subscription links
   */
  public function get_links($options = array()) {
    $links = array();
    foreach ($this->get_instances() as $subscription) {
      $links[] = $subscription->get_link(NULL, $options);
    }
    return $links;
  }
  
  /**
   * Get options form
   */
  public function get_form($type, $form, &$form_state) {
    $form['type'] = array('#type' => 'value', '#value' => $this);
    $form['subscriptions'] = array('#type' => 'value', '#value' => $this);
    $form['list'] = $this->get_elements($type);
    $form['save'] = array('#type' => 'submit', '#value' => t('Update'));
    return $form;
  }
  /**
   * Get elements
   */
  public function get_elements($type) {
    switch ($type) {
      case 'checkboxes':
      default:
        return $this->element_checkboxes();
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
  function element_checkboxes($element = array()) {
    $options = $defaults = array();
    foreach ($this->get_instances() as $index => $subscription) {
      $options[$index] = $subscription->get_name();
      if ($subscription->is_stored()) {
        $defaults[] = $index;
      }
    }
    return $element + array(
      '#type' => 'checkboxes',
      '#options' => $options,
      '#default_value' => $defaults,
    );
  }

  /**
   * Implementation of iterator interface
   */
  public function rewind() {
    $this->index = 0;
  }
  public function current() {
    $k = array_keys($this->subscriptions);
    return $this->subscriptions[$k[$this->index]];
  }
  public function key() {
    $k = array_keys($this->subscriptions);
    return $k[$this->index];
  }
  public function next() {
    $k = array_keys($this->subscriptions);
    if (isset($k[++$this->index])) {
        return $this->subscriptions[$k[$this->index]];
    } else {
        return false;
    }
  }
  public function valid() {
    $k = array_keys($this->subscriptions);
    return isset($k[$this->index]);
  }
}

/**
 * Loadable collection of subscriptions for tablesel ect
 */
class Notifications_Subscription_Table extends Notifications_Subscription_List {
  public $conditions = array();
  public $base_path = 'notifications/subscription';
  public $pager = 50;
  /**
   * Set conditions and remove fields if in the header
   */
  function set_conditions($conditions = array()) {
    $this->conditions = $conditions;
    foreach (array_keys($conditions) as $id) {
      if (isset($this->header[$id])) {
        unset($this->header[$id]);
      }
    }
    return $this;
  }
  /**
   * Produce a select table with all these subscriptions
   */
  function table_select() {
    $options = array();
    foreach ($this->get_subscriptions() as $subscription) {
      $options[$subscription->sid] = $this->subscription_fields($subscription);
    }
    return array(
      '#type' => 'tableselect',
      '#header' => $this->get_header(),
      '#options' => $options,
      '#empty' => t('No subscriptions available.'),
    );
  }
  /**
   * Fill in fields for a subscription
   */
  function subscription_fields($subs) {
    
    
    $send_methods = messaging_method_info(NULL, 'name');
    $send_intervals = notifications_send_intervals();

    $fields = array(); 
    foreach (array_keys($this->header) as $index) {
      switch ($index) {
        case 'sid':
          $value = array(
            'data' => array(
              '#type' => 'link',
              '#title' => $subs->sid,
              '#href' => $this->base_path . '/' . $subs->sid,
            ),
          );
          break;
        case 'title':
          $value = $subs->get_title();
          break;
        case 'name':
          $value = $subs->get_name();
          break;
        case 'type':
          $type_list = notifications_subscription_type(NULL, 'title');
          $value = $type_list[$subs->type];
          break;
        case 'status':
          $status_list = Notifications_Subscription::status_list();
          $value = $status_list[$subs->status];
          break;
        case 'created':
          $value  = format_date($subs->created, 'short');
          break;
        case 'uid': // User
          $value = theme('username', array('account' => $subs));
          break;
        default:
          $value = isset($subs->index) ? check_plain($subs->index) : '--';
          break;
      }
      $fields[$index] = $value;
    }
    return $fields;
  }
  /**
   * Get query for selection
   */
  function query_select() {
    // Query data with $conditions and filters
    $query = db_select('notifications_subscription', 's')->extend('PagerDefault')->extend('TableSort');
    foreach ($this->conditions as $field => $value) {
      $query->condition('s.' . $field, $value);
    }
    $query
      ->fields('s',array('sid'))
      ->limit(50)
      ->orderByHeader($this->header);
    return $query;
  }
  /**
   * Actually load all the subscriptions
   */
  function query_load() {
    if ($sids = $this->query_select()->execute()->fetchCol()) {
      $subscriptions = entity_load('notifications_subscription', $sids);
      $this->add($subscriptions);
    }
    return $this;
  }
  
  /**
   * Set fields as header
   */
  function set_header($header = NULL) {
    if (isset($header)) {
      $this->header = $header; 
    }
    else {
      $this->header = $this->get_header();
    }
    return $this;
  }
  /**
   * Set default header
   */
  function get_header() {
    return isset($this->header) ? $this->header : array(
      'sid' => array('data' => t('Id'), 'field' => 's.sid'),
      'type' => array('data' => t('Type'), 'field' => 's.type'),
      'status' => array('data' => t('Status'), 'field' => 's.status'),
      'uid' => array('data' => t('User'), 'field' => 's.uid'),
      'name' => t('Description'),
      'created' => array('data' => t('Created'), 'field' => 's.created', 'sort' => 'desc'),
    );    
  }
  
}