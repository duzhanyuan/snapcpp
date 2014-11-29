/** @preserve
 * Name: locale-timezone
 * Version: 0.0.1.19
 * Browsers: all
 * Depends: editor (>= 0.0.3.245)
 * Copyright: Copyright 2013-2014 (c) Made to Order Software Corporation  All rights reverved.
 * License: GPL 2.0
 */

//
// Inline "command line" parameters for the Google Closure Compiler
// See output of:
//    java -jar .../google-js-compiler/compiler.jar --help
//
// ==ClosureCompiler==
// @compilation_level ADVANCED_OPTIMIZATIONS
// @externs $CLOSURE_COMPILER/contrib/externs/jquery-1.9.js
// @externs plugins/output/externs/jquery-extensions.js
// @js plugins/output/output.js
// @js plugins/output/popup.js
// @js plugins/server_access/server-access.js
// @js plugins/listener/listener.js
// @js plugins/editor/editor.js
// ==/ClosureCompiler==
//
// This is not required and it may not exist at the time you run the
// JS compiler against this file (it gets generated)
// --js plugins/mimetype/mimetype-basics.js
//

/*
 * JSLint options are defined online at:
 *    http://www.jshint.com/docs/options/
 */
/*jslint nomen: true, todo: true, devel: true */
/*global snapwebsites: false, jQuery: false, FileReader: true, Blob: true */


/** \file
 * \brief Extension to the editor: Locale Timezone Widget
 *
 * This file is used to extend the editor with a timezone widget used to
 * select a continent or country and a city or region. This is a composite
 * widget which uses two dropdown widgets to offer the user a way to define
 * their timezone.
 *
 * \code
 *      .
 *      .
 *      .
 *      |
 *  +---+--------------------+
 *  |                        |
 *  |  EditorWidgetType      |
 *  |  (cannot instantiate)  |
 *  +------------------------+
 *      ^
 *      | Inherit
 *      |
 *  +---+------------------------------+
 *  |                                  |
 *  | EditorWidgetTypeLocaleTimezone   |
 *  |                                  |
 *  +----------------------------------+
 * \endcode
 *
 * See also:
 * http://stackoverflow.com/tags/timezone/info
 */


/** \brief Editor widget type for locale timezone settings.
 *
 * This class handles the locale timezone settings form. Mainly it ensures
 * that the dropdown with the list of towns only shows towns for the
 * currently selected continent.
 *
 * \code
 *  class EditorWidgetTypeLocaleTimezone extends EditorWidgetType
 *  {
 *  public:
 *      EditorWidgetTypeLocaleTimezone() : snapwebsites.EditorWidget;
 *
 *      virtual function getType() : string;
 *      virtual function initializeWidget(widget: Object) : void;
 *
 *  private:
 *      function selectTowns_(widget: snapwebsites.EditorWidget) : void;
 *  };
 * \endcode
 *
 * @constructor
 * @extends {snapwebsites.EditorWidgetType}
 * @struct
 */
snapwebsites.EditorWidgetTypeLocaleTimezone = function()
{
    snapwebsites.EditorWidgetTypeLocaleTimezone.superClass_.constructor.call(this);

    return this;
};


/** \brief Chain up the extension.
 *
 * This is the chain between this class and its super.
 */
snapwebsites.inherits(snapwebsites.EditorWidgetTypeLocaleTimezone, snapwebsites.EditorWidgetType);


/** \brief Return "hidden".
 *
 * Return the name of the hidden type.
 *
 * @return {string} The name of the hidden type.
 * @override
 */
snapwebsites.EditorWidgetTypeLocaleTimezone.prototype.getType = function() // virtual
{
    return "locale_timezone";
};


/** \brief Initialize the widget.
 *
 * This function initializes the locale timezone widget.
 *
 * @param {!Object} widget  The widget being initialized.
 * @override
 */
snapwebsites.EditorWidgetTypeLocaleTimezone.prototype.initializeWidget = function(widget) // virtual
{
    var that = this,
        editor_widget = /** @type {snapwebsites.EditorWidget} */ (widget),
        editor_form = editor_widget.getEditorForm(),
        name = editor_widget.getName(),
        continent_widget = editor_form.getWidgetByName(name + "_continent"),
        continent = continent_widget.getWidget(),
        city_widget = editor_form.getWidgetByName(name + "_city"),
        city = city_widget.getWidget();

    snapwebsites.EditorWidgetTypeLocaleTimezone.superClass_.initializeWidget.call(this, widget);

    // if continent changes, we need to update the towns
    continent.bind("widgetchange", function(e)
        {
            // fix the towns on each change
            that.selectTowns_(editor_widget);
            // define the full timezone
            that.retrieveNewValue_(editor_widget);
        });
    city.bind("widgetchange", function(e)
        {
            // define the full timezone
            that.retrieveNewValue_(editor_widget);
        });

    // properly initialize the towns widget (which by default shows all the
    // towns, which is "wrong")
    this.selectTowns_(editor_widget);
};


/** \brief Select the towns corresponding to the current continent.
 *
 * This function hides all the towns, then shows the towns that correspond
 * to the currently selected continent.
 *
 * \note
 * We want to have a way to set that up automatically in the editor.
 *
 * @param {snapwebsites.EditorWidget} editor_widget  The locale timezone widget.
 *
 * @private
 */
snapwebsites.EditorWidgetTypeLocaleTimezone.prototype.selectTowns_ = function(editor_widget)
{
    var editor_form = editor_widget.getEditorForm(),
        name = editor_widget.getName(),
        continent_widget = editor_form.getWidgetByName(name + "_continent"),
        //continent = continent_widget.getWidget(),
        continent_name = continent_widget.getValue(),
        city_widget = editor_form.getWidgetByName(name + "_city"),
        city = city_widget.getWidget();

    // hide all, then show corresponding to the current continent selection
console.log("--------------------------");
console.log("Setup cities for " + continent_name);
    if(continent_name)
    {
        city_widget.enable();
        city.find("li").hide();
        city.find("li." + continent_name).show();
        selected = city.find("li.selected");
console.log("  selected length = " + selected.length);
        if(!selected.exists() || !selected.hasClass(continent_name))
        {
console.log("  +--> setting new value: [" + city.find("li." + continent_name).first().html() + "]");
            city_widget.setValue(city.find("li." + continent_name).first().html());
        }
        // else -- user did not change continent name
else console.log("  +--> it is still visible!");
    }
    else
    {
        city_widget.disable();
        city_widget.resetValue(true);
    }
};


/** \brief Determine the new value of the timezone.
 *
 * This function gets the value of the continent and the city and
 * generates the new value of the timezone widget.
 *
 * \todo
 * Find a way to get the formchange event only if the continent or city
 * changed, and not on any change to the entire form.
 *
 * @param {snapwebsites.EditorWidget} editor_widget  The locale timezone widget.
 *
 * @private
 */
snapwebsites.EditorWidgetTypeLocaleTimezone.prototype.retrieveNewValue_ = function(editor_widget)
{
    var editor_widget_content = editor_widget.getWidgetContent(),
        editor_form = editor_widget.getEditorForm(),
        name = editor_widget.getName(),
        continent_widget = editor_form.getWidgetByName(name + "_continent"),
        //continent = continent_widget.getWidget(),
        continent_name = continent_widget.getValue(),
        city_widget = editor_form.getWidgetByName(name + "_city"),
        city_name = city_widget.getValue(),
        result_value;
        //city = city_widget.getWidget();

console.log("Save new value...");
    if(continent_name && city_name)
    {
        result_value = continent_name.replace(' ', '_') + "/" + city_name.replace(' ', '_');
        editor_widget_content.attr("value", result_value);
    }
};



// auto-initialize
jQuery(document).ready(function()
    {
        snapwebsites.EditorInstance.registerWidgetType(new snapwebsites.EditorWidgetTypeLocaleTimezone());
    });

// vim: ts=4 sw=4 et
