<?php
// $Id: notifications_queue.class.inc,v 1.1.2.10 2010/07/24 10:16:12 jareyero Exp $
/**
 * Notifications module. Queue processing.
 * 
 * @ TO DO: Support different languages for message localization
 */

// Number of users to process for each step
define('NOTIFICATIONS_STEP_ROWS', variable_get('notifications_step_rows', 1000));
define('NOTIFICATIONS_STEP_USERS', variable_get('notifications_step_users', 200));

// Minimum amount of seconds the process will need for clean-up tasks
// Just to make sure that after exhausting cron assigned time we'll have a few spare seconds for some cleanup
// Change this time for allowing more cron time to modules running cron after Notifications
define('NOTIFICATIONS_TIME_MARGIN', variable_get('notifications_time_margin', 5));

/**
 * Queue management and processing
 * 
 * Tables used here: notifications_queue, notifications_sent, notifications_event
 *
 */
class Notifications_Queue implements DrupalReliableQueueInterface {
  // Queue name, will allow different sub-queues in the main queue
  protected $name;
  // Process control variables
  public $process_limit;
  public $process_current;
  public $process_options;
  public $process_max_sqid;
  // Optional language to process only rows in this language. Not used yet.
  // This can be used to run different languages in different crons and prent static caching issues.
  public $process_language = NULL;

  /**
   * Start working with a queue.
   *
   * @param $name
   *   Arbitrary string. The name of the queue to work with.
   */
  public function __construct($name = 'notifications') {
    $this->name = $name;
  }

  /**
   * Add a queue item and store it directly to the queue.
   *
   * @param $data
   *   Arbitrary data to be associated with the new task in the queue.
   * @return
   *   TRUE if the item was successfully created and was (best effort) added
   *   to the queue, otherwise FALSE. We don't guarantee the item was
   *   committed to disk etc, but as far as we know, the item is now in the
   *   queue.
   */
  public function createItem($data) {
    $data += array(
      'name' => $this->name,
      'created' => time(),
    );
    return drupal_write_record('notifications_queue', $data);  
  }

  /**
   * Retrieve the number of items in the queue.
   *
   * This is intended to provide a "best guess" count of the number of items in
   * the queue. Depending on the implementation and the setup, the accuracy of
   * the results of this function may vary.
   *
   * e.g. On a busy system with a large number of consumers and items, the
   * result might only be valid for a fraction of a second and not provide an
   * accurate representation.
   *
   * @return
   *   An integer estimate of the number of items in the queue.
   */
  public function numberOfItems() {
    return db_query('SELECT COUNT(sqid) FROM {notifications_queue} WHERE name = :name', array(':name' => $this->name))->fetchField();
  }

  /**
   * Claim an item in the queue for processing.
   *
   * @param $lease_time
   *   How long the processing is expected to take in seconds, defaults to an
   *   hour. After this lease expires, the item will be reset and another
   *   consumer can claim the item. For idempotent tasks (which can be run
   *   multiple times without side effects), shorter lease times would result
   *   in lower latency in case a consumer fails. For tasks that should not be
   *   run more than once (non-idempotent), a larger lease time will make it
   *   more rare for a given task to run multiple times in cases of failure,
   *   at the cost of higher latency.
   * @return
   *   On success we return an item object. If the queue is unable to claim an
   *   item it returns false. This implies a best effort to retrieve an item
   *   and either the queue is empty or there is some other non-recoverable
   *   problem.
   */
  public function claimItem($lease_time = 60) {
      // Claim an item by updating its expire fields. If claim is not successful
    // another thread may have claimed the item in the meantime. Therefore loop
    // until an item is successfully claimed or we are reasonably sure there
    // are no unclaimed items left.
    while (TRUE) {
      $item = db_query_range('SELECT * FROM {notifications_queue} q WHERE expire = 0 AND name = :name ORDER BY created ASC', 0, 1, array(':name' => $this->name))->fetchObject();
      if ($item) {
        // Try to update the item. Only one thread can succeed in UPDATEing the
        // same row. We cannot rely on REQUEST_TIME because items might be
        // claimed by a single consumer which runs longer than 1 second. If we
        // continue to use REQUEST_TIME instead of the current time(), we steal
        // time from the lease, and will tend to reset items before the lease
        // should really expire.
        $update = db_update('notifications_queue')
          ->fields(array(
            'expire' => time() + $lease_time,
          ))
          ->condition('sqid', $item->sqid)
          ->condition('expire', 0);
        // If there are affected rows, this update succeeded.
        if ($update->execute()) {
          $item->data = $item->data ? unserialize($item->data) : array();     
          return $item;
        }
      }
      else {
        // No items currently available to claim.
        return FALSE;
      }
    }
  }    

  public function releaseItem($item) {
    $update = db_update('queue')
      ->fields(array(
        'expire' => 0,
      ))
      ->condition('sqid', $item->sqid);
      return $update->execute();
  }

  public function deleteItem($item) {
    db_delete('notifications_queue')
      ->condition('sqid', $item->sqid)
      ->execute();
  }

  public function createQueue() {
    // All tasks are stored in a single database table so there is nothing we need
    // to do to createa new queue.
  }

  /**
   * Delete a queue and every item in the queue.
   */
  public function deleteQueue() {
    db_delete('notifications_queue')
      ->condition('name', $this->name)
      ->execute();  
  }

  /**
   * Callback for Drupal cron
   */
  function process_cron() {
    $this->process_clean();
    $this->process_prepare();
    $this->process_control('cron');
    $this->process_run();
  }

  /**
   * Function to be called on cron by the main notifications_cron
   * 
   * It will call each subscription_process for each interval a number of times
   * 
   * This should send out messages starting with immediate delivery. We send first immediate delivery
   * because the other ones can be added up for each period. 
   * Assumption: The bigger the interval, the longer delay it may admit (?) I.e. sending hourly email 
   * after 1 hour 15 mins may be ok if the system is overloaded.
   * 
   * @return int
   *   Number of rows processed
   */
  function process_run() {
    notifications_log('Starting notifications process');

    $count = 0;
    $stop = FALSE;
    $send_intervals = _notifications_send_intervals();
    unset($send_intervals[-1]);
    
    if ($max_sqid = $this->process_prepare()) {
      foreach ($send_intervals as $interval => $name) {
        notifications_log('Processing queue', array('send interval' => $name));
        while ($rows = $this->process_queue($interval, $max_sqid)) {
          $count += $rows;
          $stop = !$this->process_control('check');
        }
        if ($stop) {
          notifications_log('Process stopped, reached processing limits');
          break;
        } else {
          notifications_log('Process finished', array('send interval' => $name));
        }
      }
    } else {
      notifications_log('No rows in queue');
    }
    $this->process_control('stop');
    return $count;
  }
  
  /**
   * Prepare subscriptions queue
   * 
   * This is intended to avoid race conditions where new rows are added while the process is running
   * 
   * @return
   *   Max $sqid that will be processed this cron
   */
  function process_prepare() {
    if (!isset($this->process_max_sqid)) {
      $this->process_control('start');
      // This will get the latest notification in queue so we don't mess with new ones being created during cron run
      // It will also prevent clashes with the immediate sending feature
      $this->process_max_sqid = db_result(db_query("SELECT max(sqid) FROM {notifications_queue}"));
    }
    return $this->process_max_sqid;
  }
  
  /**
   * Clean up queue and events before starting process
   */
  public static function process_clean() {
    // Clean up expired logs from queue if logging enabled
    if ($keep = variable_get('notifications_log', 0)) {
      db_query("DELETE FROM {notifications_queue} WHERE cron = 0 AND sent < %d", time() - $keep);
    }
    // Clean up event table
    self::event_clean();   
  }
  
  /**
   * Controls and checks limits for queue processing
   * It can be used by other modules to add their own limits here, like number of sms sent, etc...
   * @param $op
   *   'start' => Start the counters
   *   'cron' => Special time adjustment for cron operations
   *   'init' => Start a new counter with $value limit
   *   'option' => Sets /gets options
   *      - debug
   *      - output Enables output for admin page
   * @return
   *   TRUE if we are yet under the processing limits
   */
  function process_control($op = 'check', $name = NULL, $value = NULL) {
    switch ($op) {
      case 'start':
        $this->process_current = array('message' => 0, 'step' => 0, 'timer' => 0, 'row' => 0);
        $defaults = variable_get('notifications_process_limit', array('time' => 0, 'message' => 0, 'row' => 0, 'percent' => MESSAGING_DEFAULT_CRON_PERCENT));
        foreach ($defaults as $name => $value) {
          if ($value && !isset($this->process_limit[$name])) {
            $this->process_limit[$name] = $value;
          }
        }
        timer_start('notifications_process');
        break;
 
      case 'stop':
        $timer = timer_stop('notifications_process');
        $this->process_current['timer'] = $timer['time'];
        return $timer;

      case 'cron':
        // Calculate time limit. We get the smaller of all these times in seconds
        // There's an issue with poormanscron not setting the cron semaphore so it will default to current time
        $timelimit = array();
        $cronstart = variable_get('cron_semaphore', time());     
        // Max execution time may be zero meaning no limit, then no limits based on this
        if ($maxtime = ini_get('max_execution_time')) {
          $timelimit[] =  $cronstart + $maxtime - NOTIFICATIONS_TIME_MARGIN;
          if (!empty($this->process_limit['percent'])) {
            $timelimit[] = time() + $maxtime * $this->process_limit['percent'] / 100;
            unset($this->process_limit['percent']);
          }
        }
        // This is an absolute limit, applies always if set
        if (!empty($this->process_limit['time'])) {
          $timelimit[] = time() + $this->process_limit['time'];
        }
        if ($timelimit) {
          $this->process_limit['time'] = min($timelimit);
        }
        break;

      case 'init':
        $this->process_current[$name] = 0;
        $this->process_limit[$name] = $value;
        break;

      case 'current':
        // Return current value for counter
        return isset($this->process_current[$name]) ? $this->process_current[$name] : 0;

      case 'count':
        $value =  $value ? $value : 1;
        isset($this->process_current[$name]) ? ($this->process_current[$name] += $value) : $this->process_current[$name] = $value;
        break;

      case 'check':
        // Check all limits till we find a false one
        $this->process_current['time'] = time();
        if (isset($this->process_limit)) {
          foreach ($this->process_limit as $name => $value) {
            if ($value && !empty($this->process_current[$name]) && $this->process_current[$name] >= $value) {
              watchdog('notifications', 'Reached processing limit on queue processing: %name = %value', array('%name' => $name, '%value' => $value));
              return FALSE;
            }
          }
        }
        return TRUE;

      case 'results':
        // Return array of variables needed to print out some messages
        return array(
          '@rows' => $this->process_current['row'],
          '@messages' => $this->process_current['message'],
          '@time' => $this->process_current['timer'],
        );
    } 
  }
  
  /**
   * Get / set limit value for counter
   */
  function process_limit($name, $value = NULL) {
    if (isset($value)) {
      $this->process_limit[$name] = $value;
    }
    else {
      return isset($this->process_limit[$name]) ? $this->process_limit[$name] : 0;
    }    
  }
  
  /**
   * Get / set process option
   */
  function process_option($name, $value = NULL) {
    // Get / set value for option
    if (isset($value)) {
      $this->process_options[$name] = $value;
    }
    else {
      return isset($this->process_options[$name]) ? $this->process_options[$name] : FALSE;
    }    
  }  
  
  /**
   * Get message template for notifications
   * 
   * This is a hidden variable that can be overridden to use a different class: notifications_message_template
   */
  public static function message_template($params = NULL) {
    $class = variable_get('notifications_message_template', 'Notifications_Message');
    return new $class($params);
  }

  /**
   * Queue events for notifications adding query conditions from plug-ins
   * 
   * This is an example of the resulting query
   *
   * INSERT INTO {notifications_queue} (uid, sid, module, eid, send_interval, send_method, cron, created, conditions)
   * SELECT DISTINCT s.uid, s.sid, s.module, 34, s.send_interval, s.send_method, s.cron, 1230578161, s.conditions FROM notifications s 
   * INNER JOIN notifications_fields f ON s.sid = f.sid 
   * WHERE s.status = 1 AND s.event_type = 'node' AND s.send_interval >= 0 
   * AND ((f.field = 'nid' AND f.value = '2') OR (f.field = 'type' AND f.value = 'story') OR (f.field = 'author' AND f.value = '1'))
   * GROUP BY s.uid, s.sid, s.module, s.send_interval, s.send_method, s.cron, s.conditions 
   * HAVING s.conditions = count(f.sid)
   *
   * @param $event
   *   Event object
   * 
   * @return int
   *   Number of queued rows
   */
  function queue_event($event) {
    if ($event_conditions = $event->get_conditions()) {
      $insert = db_insert('notifications_queue')
        ->fields(array('name', 'uid', 'mdid', 'send_method', 'sid', 'send_interval', 'language', 'conditions', 'created', 'eid'));
      $select = db_select('notifications', 's')
        ->distinct()
        ->fields('s', array('queue', 'uid', 'mdid', 'send_method','sid', 'send_interval', 'language', 'conditions'));
      $select->addExpression(REQUEST_TIME);
      $select->addExpression($event->eid);
      $select->innerJoin('notifications_fields', 'f', 's.sid = f.sid');
      $select->groupBy('uid')
        ->groupBy('sid')
        ->groupBy('send_interval')
        ->groupBy('send_method');
      $select->condition('status', 1)
        ->condition('send_interval', 1, '>=');
      // Add all field conditions
      $fields = db_or();
      foreach ($event_conditions as $condition) {
        $fields->condition($condition);
      }
      $select->condition(db_and()->condition($fields));
      // Add metadata for altering and run
      $select->addTag('notifications queue event');
      $select->addMetaData('event', $event);
      $insert->from($select)->execute();
    notifications_include('object.inc');
    notifications_include('query.inc');
  
    $query = array();
    // Build big insert query using the query builder. The fields for this event type will be added by the plug-ins. 
    // If no arguments retrieved, skip this step
    if ($query_args = module_invoke_all('notifications_event', 'query', $event)) {
      // Build a query skeleton and add parameters for each module separately
      $query = notifications_query_event_queue($event);
      foreach ($query_args as $query_params) {
        $query = notifications_query_build($query_params, $query);
      }
      
      // Give a chance to other modules to alter the query or empty it so we don't throw it
      drupal_alter('notifications_query', $query, $event);
      
      // Finally we build the SELECT part of the query and glue it to the INSERT
      if ($query) {
        list($sql, $args) = notifications_query_sql($query);
        db_query($sql, $args);
      }
    }
    
    // Return number of queued rows
    return db_result(db_query('SELECT COUNT(*) FROM {notifications_queue} WHERE eid = %d', $event->eid));
  }
  
}