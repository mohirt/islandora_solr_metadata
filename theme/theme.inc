<?php
/**
 * @file
 * Theme functions for the islandora_solr_metadata module.
 */

/**
 * Implements theme_element().
 */
function theme_islandora_solr_metadata_management_table(array $variables) {
  $table = $variables['table'];
  $rows = array();
  $weight_class = 'islandora-solr-metadata-weight';
  foreach (element_children($table['rows']) as $id) {
    $row = &$table['rows'][$id];
    $row['weight']['#attributes'] = array('class' => array($weight_class));
    $rows[] = array(
      'data' => array(
        drupal_render($row['remove_field']),
        drupal_render($row['solr_field_markup']),
        drupal_render($row['display_label']),
        drupal_render($row['hyperlink']),
        drupal_render($row['weight']),
      ),
      'class' => array('draggable'),
    );
  }
  $table_id = 'entry-order';
  $output = theme('table', array(
    'header' => $table['#header'],
    'rows' => $rows,
    'attributes' => array('id' => $table_id),
    'empty' => t('There are no entries available.')));
  $output .= drupal_render_children($table);
  drupal_add_tabledrag($table_id, 'order', 'sibling', $weight_class);
  return $output;
}

/**
 * Prepares variables for islandora_solr_metadata_display templates.
 *
 * Default template: islandora-solr-metadata-display.tpl.php
 *
 * @param array $variables
 *   An associative array containing:
 *   - islandora_object: The AbstractObject for which we are generating a
 *     metadata display.
 *   - print: A boolean indicating to disable some functionality, to facilitate
 *     printing. When TRUE, avoids adding the "collapsible" and "collapsed"
 *     classes to fieldsets.
 */
function template_preprocess_islandora_solr_metadata_display(array &$variables) {
  module_load_include('inc', 'islandora_solr_metadata', 'includes/db');
  module_load_include('inc', 'islandora', 'includes/utilities');
  drupal_add_js('misc/form.js');
  drupal_add_js('misc/collapse.js');

  $object = $variables['islandora_object'];
  $db_fields = array();
  $solr_fields =& $variables['solr_fields'];
  $associations = islandora_solr_metadata_get_associations_by_cmodels($object->models);

  foreach ($associations as $configuration_id) {
    $field = islandora_solr_metadata_get_fields($configuration_id['configuration_id']);
    $db_fields = array_merge($db_fields, $field);
  }
  foreach ($db_fields as $solr_field => $value) {
    if (isset($solr_fields[$solr_field])) {
      continue;
    }
    // Make an array for use later on.
    $solr_fields[$solr_field] = array(
      'display_label' => $value['display_label'],
      'value' => array(),
      'hyperlink' => $value['hyperlink'],
      'weight' => $value['weight'],
    );
  }

  $variables['parent_collections'] = islandora_get_parents_from_rels_ext($object);
}

/**
 * Implements template_process_HOOK() for islandora_solr_metadata_description.
 */
function template_process_islandora_solr_metadata_display(array &$variables) {
  islandora_solr_metadata_query_fields($variables['islandora_object'], $variables['solr_fields']);
}

/**
 * Prepares variables for islandora_solr_metadata_description templates.
 *
 * Default template: islandora-solr-metadata-description.tpl.php
 *
 * @param array $variables
 *   An associative array containing:
 *   - islandora_object: The AbstractObject for which we are generating a
 *     metadata display.
 */
function template_preprocess_islandora_solr_metadata_description(array &$variables) {
  $object = $variables['islandora_object'];
  $solr_fields =& $variables['solr_fields'];

  $descriptions = array();
  module_load_include('inc', 'islandora_solr_metadata', 'includes/db');
  $associations = islandora_solr_metadata_get_associations_by_cmodels($object->models);
  foreach ($associations as $configuration_id) {
    $description = islandora_solr_metadata_retrieve_description($configuration_id['configuration_id']);
    if ($description['description_field'] !== NULL) {
      $descriptions[] = $description;
    }
  }
  foreach ($descriptions as $description) {
    $desc_field = $description['description_field'];
    $solr_fields[$desc_field] = array(
      'display_label' => $description['description_label'],
      'value' => array(),
    );
  }
}

/**
 * Implements template_process_HOOK() for islandora_solr_metadata_description.
 */
function template_process_islandora_solr_metadata_description(array &$variables) {
  islandora_solr_metadata_query_fields($variables['islandora_object'], $variables['solr_fields']);

  // XXX: Seems a little odd, yes... Was reusing the "description" value,
  // though.
  $variables['description'] = $variables['solr_fields'];
}

/**
 * Perform the query to Solr to get our metadata values.
 *
 * @param AbstractObject $object
 *   The object for which we are going to query Solr... Really only need the
 *   PID...
 * @param array $solr_fields
 *   An associative array mapping Solr fields to associative arrays, of which
 *   we use:
 *   - hyperlink: An optional boolean indicating if the given value should be
 *     rendered a link.
 *   - weight: An optional weight we will use to sort the values.
 *   - formatter: An optional callable to which we will call with each value
 *     from the given field, to return alternate text or markup as desired.
 */
function islandora_solr_metadata_query_fields($object, &$solr_fields) {
  if (!empty($solr_fields)) {
    $query_processor = new IslandoraSolrQueryProcessor();
    $query_processor->solrQuery = 'PID:"' . $object->id . '"';
    $query_processor->solrParams['fl'] = '*';
    $query_processor->executeQuery(FALSE);
    if ($query_processor->islandoraSolrResult['response']['numFound'] > 0) {
      $solr_results_doc = $query_processor->islandoraSolrResult['response']['objects']['0']['solr_doc'];

      // Ensure we only provide results for what is specified.
      $constrained_results = array_intersect_key($solr_results_doc, $solr_fields);

      foreach ($constrained_results as $solr_field => $value) {
        // Want the values in array form even if it's a single valued field.
        if (!is_array($value)) {
          $value = (array) $value;
        }
        // Trim off any extra values.
        $value = array_filter(array_map('trim', $value));
        // Make hyperlinks out of what we need to.
        if (isset($solr_fields[$solr_field]['hyperlink']) && $solr_fields[$solr_field]['hyperlink']) {
          module_load_include('inc', 'islandora_solr_search', 'includes/utilities');
          foreach ($value as &$search_value) {
            $solr_query = format_string('!field:"!value"', array(
              '!field' => $solr_field,
              '!value' => islandora_solr_lesser_escape($search_value),
            ));
            $search_value = l($search_value, "islandora/search/$solr_query");
          }
          unset($search_value);
        }

        $solr_fields[$solr_field]['value'] = $value;
      }

      if (variable_get('islandora_solr_metadata_omit_empty_values', FALSE)) {
        $filter_function = function ($field_info) {
          return !empty($field_info['value']);
        };
        $filtered = array_filter($solr_fields, $filter_function);
        $solr_fields = $filtered;
      }

      if (variable_get('islandora_solr_metadata_dedup_values', FALSE)) {
        foreach ($solr_fields as &$field) {
          $field['value'] = array_unique($field['value']);
        }
        unset($field);
      }

      foreach ($solr_fields as &$field) {
        if (isset($field['formatter']) && is_callable($field['formatter'])) {
          $field['value'] = array_map($field['formatter'], $field['value']);
        }
      }
      unset($field);

      uasort($solr_fields, 'drupal_sort_weight');
    }
    else {
      drupal_set_message(t('Unable to find @pid object in the Solr index. Contact the administrator!', array('@pid' => $object->id)), 'error');
    }
  }
}
