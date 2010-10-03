<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Templates
 */

/**
 * Base template class
 */
class Notifications_Template {
  // Current template elements, renderable
  public $elements = array();
  // Template text, to override defaults
  protected $text = array();
  // Store multiple object types
  protected $objects = array();
  protected $events = array();
  
  protected $subscriptions = array();
  // Options for string replacement
  protected $options;
  // Parent template
  protected $parent;
  
  /**
   * Constructor, from notifications event
   */
  public function __construct($event = NULL) {
    if ($event) {
      $this->objects = $event->get_objects();
      $this->objects['event'] = $event;
    }
  }
  /**
   * Add object to the list
   */
  function add_object($type, $object) {
    $this->objects[$type] = $object;
  }
  /**
   * Add element
   */
  function add_element($name, $element) {
    $this->elements[$name] = $element;
  }
  /**
   * Add template object
   */
  function add_template($name, $template) {
    $template->set_parent($this);
    $this->elements[$name] = $template;
  }
  /**
   * Add event object and its corresponding template
   */
  function add_event($event) {
    $this->events[$event->eid] = $event;
    $this->add_template('event:' . $event->eid, $event->get_template());
  }
  /**
   * Set parent template (and add objects from the parent)
   */
  function set_parent($template) {
    $this->parent = $template;
    $this->objects += $template->get_objects();
  }
  /**
   * Get objects from this template (include parent's ones)
   */
  function get_objects() {
    $objects = $this->objects;
    if (isset($this->parent)) {
      $objects += $this->parent->get_objects();
    }
    return $objects;
  }
  /**
   * Get objects as Drupal objects (Removing Notifications object wrapper)
   */
  function drupal_objects() {
    $objects = array();
    foreach ($this->get_objects() as $type => $object) {
      $objects[$type] = is_a($object, 'Notifications_Object') ? $object->get_object() : $object;
    }
    return $objects;
  }
  /**
   * Render this as text
   */
  function render() {
    $elements = $this->build() + array('#type' => 'messaging_template');
    return drupal_render($this->elements);
  }

  /**
   * Get Message_Object with subject and body parts
   */
  function build_message() {
    $message = new Messaging_Message();
    $subject = reset($this->build_parts(array('subject')));
    $body = $this->build_parts(array('header', 'content', 'footer')) + array('#type' => 'messaging_body');
    $message->set_element('subject', $subject);
    $message->set_element('body', $body);
    return $message;
  }
  /**
   * Build all elements
   */
  function build() {
    return $this->build_parts(array_keys($this->elements));
  }
  /**
   * Get multiple built elements
   * 
   * @param $key1, $key2, $key3..
   */
  function build_parts($parts) {
    $elements = array();
    foreach ($parts as $key) {
      if ($text = $this->get_element($key)) {
        $elements[$key] = $this->build_element($text);
      }
    }
    return $elements;
  }

  /**
   * Build a message element
   */
  function build_element($element, $options = array()) {
    if (is_object($element)) {
      return $element->build();
    }
    elseif (is_string($element)) {
      // Strings may need token replacement
      return array(
        '#markup' => $this->token_replace($element, $options),
      );
    }
    elseif (is_array($element)) {
      foreach (element_children($element) as $key) {
        $element[$key] = $this->build_element($element[$key], $options);
      }
      return $element;
    }
  }

  /**
   * Get element from elements or default texts
   */
  function get_element($type, $options = array()) {
    if (isset($this->elements[$type])) {
      return $this->elements[$type];
    }
    else {
      return $this->get_text($type, $options);
    }
  }
  /**
   * Get text element from this template
   */
  function get_text($type, $options = array()) {
    if (isset($this->text[$type])) {
      return $this->text[$type];
    }
    else {
      $options += $this->get_options();
      return $this->default_text($type, $options);
    }
  }
  /**
   * Get options for texts, translations, etc
   */
  function get_options() {
    if (!isset($this->options)) {
      $this->options = array(
        'language' => language_default(),
        'langcode' => language_default()->language,
      );
    }
    return $this->options;
  }
  /**
   * Default texts for this template, may need token replacement
   */
  function default_text($type, $options) {
    switch ($type) {
      case 'subject':
        return t('Notification for [user:name] from [site:name]', array(), $options);
      case 'header':
        return t("Greetings [user:name],", array(), $options);
      case 'content':
        return t("A item to which you are subscribed has been updated", array(), $options);
      case 'footer':
        return array(
          '#type' => 'messaging_text',
          'from' => t('This is an automatic message from [site:name]', array(), $options),
          'subscriptions' => t('To manage your subscriptions, browse to [notifications:subscriptions-manage]', array(), $options),
          'unsubscribe' => t('You can unsubscribe at [notifications:unsubscribe-url]', array(), $options),
        );
    }
  }
  /**
   * Do token replacement with this template's objects
   */
  protected function token_replace($text, $options = array()) {
    return token_replace($text, $this->drupal_objects(), $options + $this->get_options());
  }
}

