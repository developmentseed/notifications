<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Templates
 */

/**
 * Base template class
 */
class Notifications_Template() {
  public $elements = array();
  // Store multiple object types
  protected $objects = array();
  protected $events = array();
  protected $subscriptions = array();
  // Parent template
  protected $parent;
  
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
   * Add template
   */
  function add_template($name, $template = NULL) {
    $template->parent = $this;
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
   * Build text elements
   */
  function build() {
    
  }
  /**
   * Prepare this template for rendering
   */
  function prepare() {
    
  }
  /**
   * Render this as text
   */
  function render($elements = NULL) {
    $this->build();
    $this->render = drupal_render($this->elements);
    $this->text = $this->token_replace($this->render);
  }
  /**
   * Render this template's elements
   */
  function get_text() {
    $this->elements;
  }
  /**
   * Build message object
   */
  function get_message() {
    return messaging_message_build($this);
  }
}

