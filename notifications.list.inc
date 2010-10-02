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

