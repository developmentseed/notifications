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
   * Set user to all subscriptions
   */
  function set_user($user) {
    $this->instance_params['uid'] = $user->uid;
    foreach ($this->get_instances() as $subscription) {
      $subscription->ser_user($user);
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
 * Display list as links item list
 */
function theme_notifications_subscription_list_links($variables) {
  $subscriptions = $variables['subscriptions'];
  $items = array();
  foreach ($subscription as $subscription) {
    $items[] = $subscription->subscription_link();
  }
  dpm($items);
  if ($items) {
    return theme('item_list', array('items' => $items));
  }
}