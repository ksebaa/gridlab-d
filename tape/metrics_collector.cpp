/*
 * metrics_collector.cpp
 *
 *  Created on: Jan 30, 2017
 *      Author: tang526
 */

#include "metrics_collector.h"

#include <iostream>
#include <fstream>
#include <string.h>
#include <algorithm>
#include <cmath>
using namespace std;

CLASS *metrics_collector::oclass = NULL;
CLASS *metrics_collector::pclass = NULL;
metrics_collector *metrics_collector::defaults = NULL;
PROPERTY *metrics_collector::propTriplexNomV = NULL;
PROPERTY *metrics_collector::propTriplexV1 = NULL;
PROPERTY *metrics_collector::propTriplexV2 = NULL;
PROPERTY *metrics_collector::propTriplexV12 = NULL;
PROPERTY *metrics_collector::propTriplexPrice = NULL;
PROPERTY *metrics_collector::propTriplexP = NULL;
PROPERTY *metrics_collector::propTriplexQ = NULL;
PROPERTY *metrics_collector::propMeterNomV = NULL;
PROPERTY *metrics_collector::propMeterVa = NULL;
PROPERTY *metrics_collector::propMeterVb = NULL;
PROPERTY *metrics_collector::propMeterVc = NULL;
PROPERTY *metrics_collector::propMeterVab = NULL;
PROPERTY *metrics_collector::propMeterVbc = NULL;
PROPERTY *metrics_collector::propMeterVca = NULL;
PROPERTY *metrics_collector::propMeterPrice = NULL;
PROPERTY *metrics_collector::propMeterP = NULL;
PROPERTY *metrics_collector::propMeterQ = NULL;
PROPERTY *metrics_collector::propHouseLoad = NULL;
PROPERTY *metrics_collector::propHouseHVAC = NULL;
PROPERTY *metrics_collector::propHouseAirTemp = NULL;
PROPERTY *metrics_collector::propHouseCoolSet = NULL;
PROPERTY *metrics_collector::propHouseHeatSet = NULL;
PROPERTY *metrics_collector::propWaterLoad = NULL;
PROPERTY *metrics_collector::propInverterS = NULL;
PROPERTY *metrics_collector::propCapCountA = NULL;
PROPERTY *metrics_collector::propCapCountB = NULL;
PROPERTY *metrics_collector::propCapCountC = NULL;
PROPERTY *metrics_collector::propRegCountA = NULL;
PROPERTY *metrics_collector::propRegCountB = NULL;
PROPERTY *metrics_collector::propRegCountC = NULL;
PROPERTY *metrics_collector::propSwingSubLoad = NULL;
PROPERTY *metrics_collector::propSwingMeterS = NULL;

void new_metrics_collector(MODULE *mod){
	new metrics_collector(mod);
}

metrics_collector::metrics_collector(MODULE *mod){
	if(oclass == NULL)
	{
#ifdef _DEBUG
		gl_debug("construction metrics_collector class");
#endif
		oclass = gl_register_class(mod,"metrics_collector",sizeof(metrics_collector), PC_POSTTOPDOWN);
    if(oclass == NULL)
      GL_THROW("unable to register object class implemented by %s",__FILE__);

    if(gl_publish_variable(oclass,
			PT_double, "interval[s]", PADDR(interval_length_dbl), PT_DESCRIPTION, "Interval at which the metrics_collector output is stored in JSON format",NULL) < 1) 
			GL_THROW("unable to publish properties in %s",__FILE__);

		defaults = this;
		memset(this, 0, sizeof(metrics_collector));
  }
}

int metrics_collector::isa(char *classname){
	return (strcmp(classname, oclass->name) == 0);
}

int metrics_collector::create(){

	memcpy(this, defaults, sizeof(metrics_collector));

	time_array = NULL;
	real_power_array = NULL;
	reactive_power_array = NULL;
	voltage_vll_array = NULL;
	voltage_vln_array = NULL;
	voltage_unbalance_array = NULL;
	total_load_array = NULL;
	hvac_load_array = NULL;
	wh_load_array = NULL;
	air_temperature_array = NULL;
	dev_cooling_array = NULL;
	dev_heating_array = NULL;
	count_array = NULL;
	real_power_loss_array = NULL;
	reactive_power_loss_array = NULL;

	metrics = NULL;
	last_vol_val = -1.0; // give initial value as negative one

	// Interval related
	interval_length = -1;
	curr_index = 0;
	interval_length_dbl=-1.0;

	strcpy (parent_name, "No name given");

	propTriplexNomV = NULL;
	propTriplexV1 = NULL;
	propTriplexV2 = NULL;
	propTriplexV12 = NULL;
	propTriplexPrice = NULL;
	propTriplexP = NULL;
	propTriplexQ = NULL;

	propMeterNomV = NULL;
	propMeterVa = NULL;
	propMeterVb = NULL;
	propMeterVc = NULL;
	propMeterVab = NULL;
	propMeterVbc = NULL;
	propMeterVca = NULL;
	propMeterPrice = NULL;
	propMeterP = NULL;
	propMeterQ = NULL;

	propHouseLoad = NULL;
	propHouseHVAC = NULL;
	propHouseAirTemp = NULL;
	propHouseCoolSet = NULL;
	propHouseHeatSet = NULL;

	propWaterLoad = NULL;

	propInverterS = NULL;

	propCapCountA = NULL;
	propCapCountB = NULL;
	propCapCountC = NULL;

	propRegCountA = NULL;
	propRegCountB = NULL;
	propRegCountC = NULL;

	propSwingSubLoad = NULL;
	propSwingMeterS = NULL;

	return 1;
}

int metrics_collector::init(OBJECT *parent){

	OBJECT *obj = OBJECTHDR(this);

//	int temp = strcmp(parent->oclass->name,"triplex_meter") != 0;
	if (parent == NULL)
	{
		gl_error("metrics_collector must have a parent (triplex meter, house, waterheater, inverter, substation, or meter)");
		/*  TROUBLESHOOT
		Check the parent object of the metrics_collector.
		*/
		return 0;
	}
	parent_string = "";
	// Find parent, if not defined, or if the parent is not a supported class, throw an exception
	if (gl_object_isa(parent,"triplex_meter"))	{
		parent_string = "triplex_meter";
		if (propTriplexNomV == NULL) propTriplexNomV = gl_get_property (parent, "nominal_voltage");
		if (propTriplexPrice == NULL) propTriplexPrice = gl_get_property (parent, "price");
		if (propTriplexP == NULL) propTriplexP = gl_get_property (parent, "measured_real_power");
		if (propTriplexQ == NULL) propTriplexQ = gl_get_property (parent, "measured_reactive_power");
		if (propTriplexV1 == NULL) propTriplexV1 = gl_get_property (parent, "voltage_1");
		if (propTriplexV2 == NULL) propTriplexV2 = gl_get_property (parent, "voltage_2");
		if (propTriplexV12 == NULL) propTriplexV12 = gl_get_property (parent, "voltage_12");
	} else if (gl_object_isa(parent, "house")) {
		parent_string = "house";
		if (propHouseLoad == NULL) propHouseLoad = gl_get_property (parent, "total_load");
		if (propHouseHVAC == NULL) propHouseHVAC = gl_get_property (parent, "hvac_load");
		if (propHouseAirTemp == NULL) propHouseAirTemp = gl_get_property (parent, "air_temperature");
		if (propHouseCoolSet == NULL) propHouseCoolSet = gl_get_property (parent, "cooling_setpoint");
		if (propHouseHeatSet == NULL) propHouseHeatSet = gl_get_property (parent, "heating_setpoint");
	} else if (gl_object_isa(parent,"waterheater")) {
		parent_string = "waterheater";
		if (propWaterLoad == NULL) propWaterLoad = gl_get_property (parent, "actual_load");
	} else if (gl_object_isa(parent,"inverter")) {
		parent_string = "inverter";
		if (propInverterS == NULL) propInverterS = gl_get_property (parent, "VA_Out");
	} else if (gl_object_isa(parent,"capacitor")) {
		parent_string = "capacitor";
		if (propCapCountA == NULL) propCapCountA = gl_get_property (parent, "cap_A_switch_count");
		if (propCapCountB == NULL) propCapCountB = gl_get_property (parent, "cap_B_switch_count");
		if (propCapCountC == NULL) propCapCountC = gl_get_property (parent, "cap_C_switch_count");
	} else if (gl_object_isa(parent,"regulator")) {
		parent_string = "regulator";
		if (propRegCountA == NULL) propRegCountA = gl_get_property (parent, "tap_A_change_count");
		if (propRegCountB == NULL) propRegCountB = gl_get_property (parent, "tap_B_change_count");
		if (propRegCountC == NULL) propRegCountC = gl_get_property (parent, "tap_C_change_count");
	} else if (gl_object_isa(parent,"substation")) {  // must be a swing bus
		PROPERTY *pval = gl_get_property(parent,"bustype");
		if ((pval==NULL) || (pval->ptype!=PT_enumeration))
		{
			GL_THROW("metrics_collector:%s failed to map bustype variable from %s",obj->name?obj->name:"unnamed",parent->name?parent->name:"unnamed");
			/*  TROUBLESHOOT
			While attempting to set up the deltamode interfaces and calculations with powerflow, the required interface could not be mapped.
			Please check your GLM and try again.  If the error persists, please submit a trac ticket with your code.
			*/
		}
		//Map to the intermediate
		enumeration *meter_bustype = (enumeration*)GETADDR(parent,pval);
		// Check if the parent meter is a swing bus (2) or not
		if (*meter_bustype != 2) {
			gl_error("If a metrics_collector is attached to a substation, it must be a SWING bus");
			return 0;
		}
		parent_string = "swingbus";
		if (propSwingSubLoad == NULL) propSwingSubLoad = gl_get_property (parent, "distribution_load");
	} else if (gl_object_isa(parent,"meter")) {
		parent_string = "meter"; // unless it's a swing bus
		if (propMeterNomV == NULL) propMeterNomV = gl_get_property (parent, "nominal_voltage");
		if (propMeterPrice == NULL) propMeterPrice = gl_get_property (parent, "price");
		if (propMeterP == NULL) propMeterP = gl_get_property (parent, "measured_real_power");
		if (propMeterQ == NULL) propMeterQ = gl_get_property (parent, "measured_reactive_power");
		if (propMeterVa == NULL) propMeterVa = gl_get_property (parent, "voltage_A");
		if (propMeterVb == NULL) propMeterVb = gl_get_property (parent, "voltage_B");
		if (propMeterVc == NULL) propMeterVc = gl_get_property (parent, "voltage_C");
		if (propMeterVab == NULL) propMeterVab = gl_get_property (parent, "voltage_AB");
		if (propMeterVbc == NULL) propMeterVbc = gl_get_property (parent, "voltage_BC");
		if (propMeterVca == NULL) propMeterVca = gl_get_property (parent, "voltage_CA");
		PROPERTY *pval = gl_get_property(parent,"bustype");
		if ((pval!=NULL) && (pval->ptype==PT_enumeration))
		{
			enumeration *meter_bustype = (enumeration*)GETADDR(parent,pval);
			if (*meter_bustype == 2) {
				parent_string = "swingbus";
				if (propSwingMeterS == NULL) propSwingMeterS = gl_get_property (parent, "measured_power");
			}
		}
	} else {
		gl_error("metrics_collector allows only these parents: triplex meter, house, waterheater, inverter, substation, meter, capacitor, regulator");
		/*  TROUBLESHOOT
		Check the parent object of the metrics_collector.
		*/
		return 0;
	}

	// Create the structures for JSON outputs
	if (strcmp(parent_string, "triplex_meter") == 0)
	{
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		metrics = (double *)gl_malloc(MTR_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	}
	else if (strcmp(parent_string, "meter") == 0)
	{
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		metrics = (double *)gl_malloc(MTR_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	} 
	else if (strcmp(parent_string, "house") == 0)
	{
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		metrics = (double *)gl_malloc(HSE_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	}
	else if (strcmp(parent_string, "waterheater") == 0) 
	{
		if (parent->parent->name != NULL) {
			strcpy (parent_name, parent->parent->name);
		}
		metrics = (double *)gl_malloc(WH_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
		// Get the name of the waterheater for actual load
		char tname[32];
		sprintf(tname, "%i", parent->id);
		char *namestr = (parent->name ? parent->name : tname);
		sprintf(waterheaterName, "waterheater_%s_actual_load", namestr);
	}
	else if (strcmp(parent_string, "inverter") == 0)
	{
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		metrics = (double *)gl_malloc(INV_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	}
	else if (strcmp(parent_string, "capacitor") == 0)
	{
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		metrics = (double *)gl_malloc(CAP_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	}
	else if (strcmp(parent_string, "regulator") == 0)
	{
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		metrics = (double *)gl_malloc(REG_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	}
	// If its parent is a meter - this is only allowed for the Swing-bus type meter
	// TODO: we need to support non-triplex billing meters as well
	else if (strcmp(parent_string, "swingbus") == 0)
	{
		// In this work, only when a metrics_collector is attached to a swing-bus, the feeder losses are recorded
		// Find all the link objects for collecting loss values
		link_objects = gl_find_objects(FL_NEW,FT_MODULE,SAME,"powerflow",FT_END); //find all link objects
		if(link_objects == NULL){
			gl_warning("No link objects were found.");
			/* TROUBLESHOOT
			No link objects were found in your .glm file. In this way, there is no losses calculated.
			*/
		}
		if (parent->name != NULL) {
			strcpy (parent_name, parent->name);
		}
		else {
			strcpy (parent_name, "Swing Bus Metrics");
		}
		metrics = (double *)gl_malloc(FDR_ARRAY_SIZE*sizeof(double));
		if (metrics == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate JSON metrics array",obj->id);
		}
	}

	// Check valid metrics_collector output interval
	interval_length = (TIMESTAMP)(interval_length_dbl);
	if(interval_length <= 0){
		gl_error("metrics_collector::init(): invalid interval of %i, must be greater than 0", interval_length);
		/* TROUBLESHOOT
			The metrics_collector interval must be a positive number of seconds.
		*/
		return 0;
	}

	// need the time sample points for every parent type
	time_array = (TIMESTAMP *)gl_malloc(interval_length*sizeof(TIMESTAMP));
	if (time_array == NULL)
	{
		GL_THROW("metrics_collector %d::init(): Failed to allocate time array",obj->id);
		/*  TROUBLESHOOT
		While attempting to allocate the array, an error was encountered.
		Please try again.  If the error persists, please submit a bug report via the Trac system.
		*/
	}

	// Allocate the arrays based on the parent type
	if ((strcmp(parent_string, "triplex_meter") == 0) || (strcmp(parent_string, "meter") == 0)) {
		// Allocate real power array
		real_power_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (real_power_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate real power array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate reactive power array
		reactive_power_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (reactive_power_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate reactive power array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate phase 1-2 voltage array
		voltage_vll_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (voltage_vll_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate phase 1-2 voltage array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate average voltage array
		voltage_vln_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (voltage_vln_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate average vln voltage array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate voltage unbalance array
		voltage_unbalance_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (voltage_unbalance_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate voltage unbalance array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Initialize the array
		for (curr_index=0; curr_index<interval_length; curr_index++)
		{
			real_power_array[curr_index] = 0.0;
			reactive_power_array[curr_index] = 0.0;
			voltage_vll_array[curr_index] = 0.0;
			voltage_vln_array[curr_index] = 0.0;
			voltage_unbalance_array[curr_index] = 0.0;
		}

	}
	// If parent is house
	else if (strcmp(parent_string, "house") == 0) {
		// Allocate total_load array
		total_load_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (total_load_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate total_load array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate hvac_load array
		hvac_load_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (hvac_load_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate hvac_load array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate air temperature array
		air_temperature_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (air_temperature_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate air_temperature array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate air temperature deviation from cooling setpointarray
		dev_cooling_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (dev_cooling_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate dev_cooling_array array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate air temperature deviation from heating setpointarray
		dev_heating_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (dev_heating_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate dev_heating_array array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Initialize the array
		for (curr_index=0; curr_index<interval_length; curr_index++)
		{
			total_load_array[curr_index] = 0.0;
			hvac_load_array[curr_index] = 0.0;
			air_temperature_array[curr_index] = 0.0;
			dev_cooling_array[curr_index] = 0.0;
			dev_heating_array[curr_index] = 0.0;
		}
	}
	// If parent is waterheater
	else if (strcmp(parent_string, "waterheater") == 0) {
		// Allocate hvac_load array
		wh_load_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (wh_load_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate wh_load array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}
	}
	// If parent is inverter
	else if (strcmp(parent_string, "inverter") == 0) {
		// Allocate real power array
		real_power_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (real_power_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate real power array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate reactive power array
		reactive_power_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (reactive_power_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate reactive power array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}
	}
	// If parent is meter
	else if (strcmp(parent_string, "swingbus") == 0) {
		// Allocate real power array
		real_power_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (real_power_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate real power array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate reactive power array
		reactive_power_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (reactive_power_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate reactive power array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}
		// Allocate real power loss array
		real_power_loss_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (real_power_loss_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate real power loss array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}

		// Allocate reactive power loss array
		reactive_power_loss_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (reactive_power_loss_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate reactive power loss array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}
	}
	else if ((strcmp(parent_string, "capacitor") == 0) || (strcmp(parent_string, "regulator") == 0)) {
		count_array = (double *)gl_malloc(interval_length*sizeof(double));
		// Check
		if (count_array == NULL)
		{
			GL_THROW("metrics_collector %d::init(): Failed to allocate operation count array",obj->id);
			/*  TROUBLESHOOT
			While attempting to allocate the array, an error was encountered.
			Please try again.  If the error persists, please submit a bug report via the Trac system.
			*/
		}
		for (int i = 0; i < interval_length; i++) count_array[i] = 0.0;
	}
	// else not possible come to this step
	else {
		gl_error("metrics_collector must have a triplex meter, meter, house, waterheater, inverter or swing-bus as its parent");
		/*  TROUBLESHOOT
		Check the parent object of the metrics_collector. The metrics_collector is only able to be childed via a
		triplex meter or a house or an inverter when connecting into powerflow systems.
		*/
		return 0;
	}


	// Initialize tracking variables
	curr_index = 0;

	// Update time variables
	last_write = gl_globalclock;
	next_write = gl_globalclock + interval_length;
	// Record the starting time
	start_time = gl_globalclock;

	return 1;
}

TIMESTAMP metrics_collector::postsync(TIMESTAMP t0, TIMESTAMP t1) {
	// recalculate next_time, since we know commit() will fire
	if (next_write <= t1) {
		write_now = true;  // actually done below, in commit
	}
	return t1+1;  // TODO: this does not fire commit 1s later, if minstep is 60s
}

int metrics_collector::commit(TIMESTAMP t1){
	OBJECT *obj = OBJECTHDR(this);

  // read the parameters for each time step
  if(0 == read_line(obj)){
		gl_error("metrics_collector::commit(): error when reading the values");
    return 0;
  }

	// if periodic interval, check for write
	if (write_now) {
    last_write = t1;
    next_write = t1 + interval_length;  // TODO - don't we actually want last_write + interval_length
		if (0 == write_line(t1, obj)) {
			gl_error("metrics_collector::commit(): error when writing the values to JSON format");
			return 0;
		}
		write_now = false;
    curr_index = 0;  // TODO - reset the collection interval
	}

	return 1;
}

/**
	@return 0 on failure, 1 on success
 **/
int metrics_collector::read_line(OBJECT *obj){

	// save the actual simulator time
	time_array[curr_index] = gl_globalclock;

	if (strcmp(parent_string, "triplex_meter") == 0) {
		real_power_array[curr_index]  = *gl_get_double(obj->parent, propTriplexP);
		reactive_power_array[curr_index] = *gl_get_double(obj->parent, propTriplexQ);

		// Get bill value, price unit given in triplex_meter is [$/kWh]
		price_parent = *gl_get_double(obj->parent, propTriplexPrice);

		// Get voltage values, s1 to ground, s2 to ground, s1 to s2
		double v1 = (*gl_get_complex(obj->parent, propTriplexV1)).Mag();  
		double v2 = (*gl_get_complex(obj->parent, propTriplexV2)).Mag();  
		double v12 = (*gl_get_complex(obj->parent, propTriplexV12)).Mag();

		// If it is at the starting time, record the voltage for violation analysis
		if (start_time == gl_globalclock) {
			last_vol_val = fabs(v12);
		}
		// compliance with C84.1; unbalance defined as max deviation from average / average, here based on 1-N and 2-N
		double vavg = 0.5 * (v1 + v2);

		voltage_vll_array[curr_index] = fabs(v12);
		voltage_vln_array[curr_index] = vavg;
		voltage_unbalance_array[curr_index] =  0.5 * fabs(v1 - v2)/vavg;
	}
	else if (strcmp(parent_string, "meter") == 0)
	{
		real_power_array[curr_index] = *gl_get_double(obj->parent, propMeterP);
		reactive_power_array[curr_index] = *gl_get_double(obj->parent, propMeterQ);

		// Get bill value, price unit given is [$/kWh]
		price_parent = *gl_get_double(obj->parent, propMeterPrice);

		// assuming these are three-phase loads; this is the only difference with triplex meters 
		double va = (*gl_get_complex(obj->parent, propMeterVa)).Mag();   
		double vb = (*gl_get_complex(obj->parent, propMeterVb)).Mag();   
		double vc = (*gl_get_complex(obj->parent, propMeterVc)).Mag();
		double vavg = (va + vb + vc) / 3.0;
		double vmin = va;
		double vmax = va;
		if (vb < vmin) vmin = vb;
		if (vb > vmax) vmax = vb;
		if (vc < vmin) vmin = vc;
		if (vc > vmax) vmax = vc;
		double vab = (*gl_get_complex(obj->parent, propMeterVab)).Mag();   
		double vbc = (*gl_get_complex(obj->parent, propMeterVbc)).Mag();   
		double vca = (*gl_get_complex(obj->parent, propMeterVca)).Mag();
		double vll = (vab + vbc + vca) / 3.0;
		// determine unbalance per C84.1
		double vdev = fabs(vab - vll);
		double vdev2 = fabs(vbc - vll);
		double vdev3 = fabs(vca - vll);
		if (vdev2 > vdev) vdev = vdev2;
		if (vdev3 > vdev) vdev = vdev3;

		// If it is at the starting time, record the voltage for violation analysis
		if (start_time == gl_globalclock) {
			last_vol_val = vll;
		}

		voltage_vll_array[curr_index] = vll;  // Vll
		voltage_vln_array[curr_index] = vavg;  // Vln
		voltage_unbalance_array[curr_index] = vdev / vll; // max deviation from Vll / average Vll
	} 
	else if (strcmp(parent_string, "house") == 0)
	{
		total_load_array[curr_index] = *gl_get_double(obj->parent, propHouseLoad);
		hvac_load_array[curr_index] = *gl_get_double(obj->parent, propHouseHVAC);
		air_temperature_array[curr_index] = *gl_get_double(obj->parent, propHouseAirTemp);
		dev_cooling_array[curr_index] = *gl_get_double(obj->parent, propHouseCoolSet);
		dev_heating_array[curr_index] = *gl_get_double(obj->parent, propHouseHeatSet);
	}
	else if (strcmp(parent_string, "waterheater") == 0) {
		wh_load_array[curr_index] = *gl_get_double(obj->parent, propWaterLoad);
	}
	else if (strcmp(parent_string, "inverter") == 0) {
		complex VAOut = *gl_get_complex(obj->parent, propInverterS);
		real_power_array[curr_index] = (double)VAOut.Re();
		reactive_power_array[curr_index] = (double)VAOut.Im();
	}
	else if (strcmp(parent_string, "capacitor") == 0) {
		double opcount = *gl_get_double(obj->parent, propCapCountA)
			+ *gl_get_double(obj->parent, propCapCountB) + *gl_get_double(obj->parent, propCapCountC);
		count_array[curr_index] = opcount;
	}
	else if (strcmp(parent_string, "regulator") == 0) {
		double opcount = *gl_get_double(obj->parent, propRegCountA)
			+ *gl_get_double(obj->parent, propRegCountB) + *gl_get_double(obj->parent, propRegCountC);
		count_array[curr_index] = opcount;
	}
	else if (strcmp(parent_string, "swingbus") == 0) {
		// Get VAfeeder values
		complex VAfeeder;
		if (gl_object_isa(obj->parent,"substation")) {
			VAfeeder = *gl_get_complex(obj->parent, propSwingSubLoad);
		} else {
			VAfeeder = *gl_get_complex(obj->parent, propSwingMeterS);
		}
		real_power_array[curr_index] = (double)VAfeeder.Re();
		reactive_power_array[curr_index] = (double)VAfeeder.Im();
		// Get feeder loss values
		// Losses calculation
		int index = 0;
		obj = NULL;
		complex lossesSum = 0.0;
		while(obj = gl_find_next(link_objects,obj)){
			if(index >= link_objects->hit_count){
				break;
			}

			char * oclassName = obj->oclass->name;
			if (strcmp(oclassName, "overhead_line") == 0 || strcmp(oclassName, "underground_line") == 0 || strcmp(oclassName, "triplex_line") == 0 || strcmp(oclassName, "transformer") == 0 || strcmp(oclassName, "regulator") == 0 || strcmp(oclassName, "switch") == 0 || strcmp(oclassName, "fuse") == 0) {

				// Obtain the link data
				link_object *one_link = OBJECTDATA(obj,link_object);
				if(one_link == NULL){
					gl_error("Unable to map the object as link.");
					return 0;
				}

				// Calculate loss
				complex loss = one_link->power_loss;

				// real power losses should be positive
				if (loss.Re() < 0) {
					loss.Re() = -loss.Re();
				}

				lossesSum += loss;

			}

			// Increase the index value
			index++;
		}
		// Put the loss value into the array
		real_power_loss_array[curr_index] = (double)lossesSum.Re();
		reactive_power_loss_array[curr_index] = (double)lossesSum.Im();
	}
	// else not possible come to this step
	else {
		gl_error("metrics_collector must have a triplex meter, house, waterheater, inverter, swing-bus, capacitor or regulator as its parent");
		/*  TROUBLESHOOT
		Check the parent object of the metrics_collector. The metrics_collector is only able to be childed via a
		triplex meter or a house or a waterheater or an inverter or a swing-bus when connecting into powerflow systems.
		*/
		return 0;
	}


	// Update index value
	++curr_index;
	if (curr_index == interval_length) {
		curr_index = 0; // TODO: this should throw an error
	}
	return 1;
}


/**
	@return 1 on successful write, 0 on unsuccessful write, error, or when not ready
 **/

int metrics_collector::write_line(TIMESTAMP t1, OBJECT *obj){
	// In the metrics_collector object, values are rearranged in write_line into dictionary
	// Writing to JSON output file is executed in metrics_collector_writer object
	char time_str[64];
	DATETIME dt;
	double svP, svQ, svPL, svQL, svHVAC, svTotal, svWH, svAir; // these are to wrap arrays that were passed to findMedian

	if ((strcmp(parent_string, "triplex_meter") == 0) || (strcmp(parent_string, "meter") == 0)) {
		svP = real_power_array[curr_index - 1];
		svQ = reactive_power_array[curr_index - 1];
		// Rearranging the arrays of data, and put into the dictionary
		// Real power data
		metrics[MTR_MIN_REAL_POWER] = findMin(real_power_array, curr_index);
		metrics[MTR_MAX_REAL_POWER] = findMax(real_power_array, curr_index);
		metrics[MTR_AVG_REAL_POWER] = findAverage(real_power_array, curr_index);

		// Reactive power data
		metrics[MTR_MIN_REAC_POWER] = findMin(reactive_power_array, curr_index);
		metrics[MTR_MAX_REAC_POWER] = findMax(reactive_power_array, curr_index);
		metrics[MTR_AVG_REAC_POWER] = findAverage(reactive_power_array, curr_index);

		// Energy data
		metrics[MTR_REAL_ENERGY] = findAverage(real_power_array, curr_index) * interval_length / 3600;
		metrics[MTR_REAC_ENERGY] = findAverage(reactive_power_array, curr_index) * interval_length / 3600;

		// Bill - TODO?
		metrics[MTR_BILL] = metrics[MTR_REAL_ENERGY] * price_parent / 1000; // price unit given is [$/kWh]

		// Phase 1 to 2 voltage data
		metrics[MTR_MIN_VLL] = findMin(voltage_vll_array, curr_index);
		metrics[MTR_MAX_VLL] = findMax(voltage_vll_array, curr_index);
		metrics[MTR_AVG_VLL] = findAverage(voltage_vll_array, curr_index);

		// Phase 1 to 2 average voltage data
		metrics[MTR_MIN_VLN] = findMin(voltage_vln_array, curr_index);
		metrics[MTR_MAX_VLN] = findMax(voltage_vln_array, curr_index);
		metrics[MTR_AVG_VLN] = findAverage(voltage_vln_array, curr_index);

		// Voltage unbalance data
		metrics[MTR_MIN_VUNB] = findMin(voltage_unbalance_array, curr_index);
		metrics[MTR_MAX_VUNB] = findMax(voltage_unbalance_array, curr_index);
		metrics[MTR_AVG_VUNB] = findAverage(voltage_unbalance_array, curr_index);

		// Voltage above/below ANSI C84 A/B Range
		double normVol = 1.0;
		if (strcmp(parent_string, "triplex_meter") == 0) {
			normVol = *gl_get_double(obj->parent, propTriplexNomV);
		} else {
			normVol = *gl_get_double(obj->parent, propMeterNomV);
		}
		double aboveRangeA = normVol * 1.05 * (std::sqrt(3));
		double belowRangeA = normVol * 0.95 * (std::sqrt(3));
		double aboveRangeB = normVol * 1.058 * (std::sqrt(3));
		double belowRangeB = normVol * 0.917 * (std::sqrt(3));
		// Voltage above Range A
		struct vol_violation vol_Vio = findOutLimit(last_vol_val, voltage_vll_array, true, aboveRangeA, curr_index);
		metrics[MTR_ABOVE_A_DUR] = vol_Vio.durationViolation;
		metrics[MTR_ABOVE_A_CNT] = vol_Vio.countViolation;
		// Voltage below Range A
		vol_Vio = findOutLimit(last_vol_val, voltage_vll_array, false, belowRangeA, curr_index);
		metrics[MTR_BELOW_A_DUR] = vol_Vio.durationViolation;
		metrics[MTR_BELOW_A_CNT] = vol_Vio.countViolation;
		// Voltage above Range B
		vol_Vio = findOutLimit(last_vol_val, voltage_vll_array, true, aboveRangeB, curr_index);
		metrics[MTR_ABOVE_B_DUR] = vol_Vio.durationViolation;
		metrics[MTR_ABOVE_B_CNT] = vol_Vio.countViolation;
		// Voltage below Range B
		vol_Vio = findOutLimit(last_vol_val, voltage_vll_array, false, belowRangeB, curr_index);
		metrics[MTR_BELOW_B_DUR] = vol_Vio.durationViolation;
		metrics[MTR_BELOW_B_CNT] = vol_Vio.countViolation;

		// Voltage below 10% of the norminal voltage rating
		double belowNorVol10 = normVol * 0.1;
		vol_Vio = findOutLimit(last_vol_val, voltage_vll_array, false, belowNorVol10, curr_index);
		metrics[MTR_BELOW_10_DUR] = vol_Vio.durationViolation;
		metrics[MTR_BELOW_10_CNT] = vol_Vio.countViolation;

		// Update the lastVol value based on this metrics interval value
		last_vol_val = voltage_vll_array[curr_index - 1];

		// wrap the arrays for next collection interval (TODO circular queue)
		real_power_array[0] = svP;
		reactive_power_array[0] = svQ;
		voltage_vll_array[0] = voltage_vll_array[curr_index - 1];
		voltage_vln_array[0] = voltage_vln_array[curr_index - 1];
		voltage_unbalance_array[0] = voltage_unbalance_array[curr_index - 1];
	}
	// If parent is house
	else if (strcmp(parent_string, "house") == 0) {
		svTotal = total_load_array[curr_index - 1];
		svHVAC = hvac_load_array[curr_index - 1];
		svAir = air_temperature_array[curr_index - 1];
		// Rearranging the arrays of data, and put into the dictionary
		// total_load data
		metrics[HSE_MIN_TOTAL_LOAD] = findMin(total_load_array, curr_index);
		metrics[HSE_MAX_TOTAL_LOAD] = findMax(total_load_array, curr_index);
		metrics[HSE_AVG_TOTAL_LOAD] = findAverage(total_load_array, curr_index);

		// hvac_load data
		metrics[HSE_MIN_HVAC_LOAD] = findMin(hvac_load_array, curr_index);
		metrics[HSE_MAX_HVAC_LOAD] = findMax(hvac_load_array, curr_index);
		metrics[HSE_AVG_HVAC_LOAD] = findAverage(hvac_load_array, curr_index);

		// air_temperature data
		metrics[HSE_MIN_AIR_TEMP] = findMin(air_temperature_array, curr_index);
		metrics[HSE_MAX_AIR_TEMP] = findMax(air_temperature_array, curr_index);
		metrics[HSE_AVG_AIR_TEMP] = findAverage(air_temperature_array, curr_index);
		metrics[HSE_AVG_DEV_COOLING] = findAverage(dev_cooling_array, curr_index);
		metrics[HSE_AVG_DEV_HEATING] = findAverage(dev_heating_array, curr_index);

		// wrap the arrays for next collection interval (TODO circular queue)
		total_load_array[0] = svTotal;
		hvac_load_array[0] = svHVAC;
		air_temperature_array[0] = svAir;
		dev_cooling_array[0] = dev_cooling_array[curr_index - 1];
		dev_heating_array[0] = dev_heating_array[curr_index - 1];
	}
	// If parent is waterheater
	else if (strcmp(parent_string, "waterheater") == 0) {
		svWH = wh_load_array[curr_index - 1];
		// Rearranging the arrays of data, and put into the dictionary
		// wh_load data
		metrics[WH_MIN_ACTUAL_LOAD] = findMin(wh_load_array, curr_index);
		metrics[WH_MAX_ACTUAL_LOAD] = findMax(wh_load_array, curr_index);
		metrics[WH_AVG_ACTUAL_LOAD] = findAverage(wh_load_array, curr_index);

		// wrap the arrays for next collection interval (TODO circular queue)
		wh_load_array[0] = svWH;
	}
	else if (strcmp(parent_string, "inverter") == 0) {
		svP = real_power_array[curr_index - 1];
		svQ = reactive_power_array[curr_index - 1];
		// Rearranging the arrays of data, and put into the dictionary
		// real power data
		metrics[INV_MIN_REAL_POWER] = findMin(real_power_array, curr_index);
		metrics[INV_MAX_REAL_POWER] = findMax(real_power_array, curr_index);
		metrics[INV_AVG_REAL_POWER] = findAverage(real_power_array, curr_index);
		// Reactive power data
		metrics[INV_MIN_REAC_POWER] = findMin(reactive_power_array, curr_index);
		metrics[INV_MAX_REAC_POWER] = findMax(reactive_power_array, curr_index);
		metrics[INV_AVG_REAC_POWER] = findAverage(reactive_power_array, curr_index);
		// wrap the arrays for next collection interval (TODO circular queue)
		real_power_array[0] = svP;
		reactive_power_array[0] = svQ;
	}
	else if (strcmp(parent_string, "capacitor") == 0) {
		metrics[CAP_OPERATION_CNT] = findMax(count_array, curr_index);
		// wrap the arrays for next collection interval (TODO circular queue) 
		count_array[0] = count_array[curr_index - 1];
	}
	else if (strcmp(parent_string, "regulator") == 0) {
		metrics[REG_OPERATION_CNT] = findMax(count_array, curr_index);
		// wrap the arrays for next collection interval (TODO circular queue) 
		count_array[0] = count_array[curr_index - 1];
	}
	else if (strcmp(parent_string, "swingbus") == 0) {
		svP = real_power_array[curr_index - 1];
		svQ = reactive_power_array[curr_index - 1];
		svPL = real_power_loss_array[curr_index - 1];
		svQL = reactive_power_loss_array[curr_index - 1];
		// real power data
		metrics[FDR_MIN_REAL_POWER] = findMin(real_power_array, curr_index);
		metrics[FDR_MAX_REAL_POWER] = findMax(real_power_array, curr_index);
		metrics[FDR_AVG_REAL_POWER] = findAverage(real_power_array, curr_index);
		metrics[FDR_MED_REAL_POWER] = findMedian(real_power_array, curr_index);
		// Reactive power data    
		metrics[FDR_MIN_REAC_POWER] = findMin(reactive_power_array, curr_index);
		metrics[FDR_MAX_REAC_POWER] = findMax(reactive_power_array, curr_index);
		metrics[FDR_AVG_REAC_POWER] = findAverage(reactive_power_array, curr_index);
		metrics[FDR_MED_REAC_POWER] = findMedian(reactive_power_array, curr_index);
		// Energy data
		metrics[FDR_REAL_ENERGY] = findAverage(real_power_array, curr_index) * interval_length / 3600;
		metrics[FDR_REAC_ENERGY] = findAverage(reactive_power_array, curr_index) * interval_length / 3600;
		// real power loss data
		metrics[FDR_MIN_REAL_LOSS] = findMin(real_power_loss_array, curr_index);
		metrics[FDR_MAX_REAL_LOSS] = findMax(real_power_loss_array, curr_index);
		metrics[FDR_AVG_REAL_LOSS] = findAverage(real_power_loss_array, curr_index);
		metrics[FDR_MED_REAL_LOSS] = findMedian(real_power_loss_array, curr_index);
		// Reactive power data    
		metrics[FDR_MIN_REAC_LOSS] = findMin(reactive_power_loss_array, curr_index);
		metrics[FDR_MAX_REAC_LOSS] = findMax(reactive_power_loss_array, curr_index);
		metrics[FDR_AVG_REAC_LOSS] = findAverage(reactive_power_loss_array, curr_index);
		metrics[FDR_MED_REAC_LOSS] = findMedian(reactive_power_loss_array, curr_index);

		// wrap the arrays for next collection interval (TODO circular queue)
		real_power_array[0] = svP;
		reactive_power_array[0] = svQ;
		real_power_loss_array[0] = svPL;
		reactive_power_loss_array[0] = svQL;
	}

	return 1;
}

void metrics_collector::interpolate(double array[], int idx1, int idx2, double val2)
{
	array[idx2] = val2;
	int steps = idx2 - idx1;

	if (steps > 1) {
		double val1 = array[idx1];
		double dVal = (val2 - val1) / steps;
		for (int i = idx1 + 1; i < idx2; i++)
		{
			val1 += dVal;
			array[i] = val1;
		}
	}
}

double metrics_collector::findMax(double array[], int length) {
	double max = array[0];

	for (int i = 1; i < length; i++) {
		if (array[i] > max) {
			max = array[i];
		}
	}

	return max;
}

double metrics_collector::findMin(double array[], int length) {
	double min = array[0];

	for (int i = 1; i < length; i++) {
		if (array[i] < min) {
			min = array[i];
		}
	}

	return min;
}

double metrics_collector::findAverage(double array[], int length) {
	if (length < 2) return array[0];
	double sum = 0;
	double tlen = time_array[length-1] - time_array[0];
	for (int i = 1; i < length; i++) {
		double dA = 0.5 * (array[i] + array[i-1]) * (time_array[i] - time_array[i-1]);
		sum += dA;
	}
	return sum / tlen;
}

double metrics_collector::findMedian(double array[], int length) {
	double median = 0;

	std::sort(&array[0], &array[length]);

	median = length % 2 ? array[length / 2] : (array[length / 2 - 1] + array[length / 2]) / 2;

	return median;
}

vol_violation metrics_collector::findOutLimit(double lastVol, double array[], bool checkAbove, double limitVal, int length) {
	struct vol_violation result;
	int count = 0;
	double durationTime = 0.0;
	int pastVal;

	// Check the first index value
	if (array[0] > limitVal) {
		pastVal = 1;
	}
	else if (array[0] == limitVal) {
		pastVal = 0;
		count++;
	}
	else {
		pastVal = -1;
	}

	// Check length
	if (length <= 1) {
		result.countViolation = 0;
		result.durationViolation = 0.0;
		return result;
	}

	// Loop through the array serachinig for voltage violation time steps
	for (int i = 1; i < length; i++) {
		// If the value is out of the limit
		if (array[i] > limitVal) {
			// If at last time step, the value was out of the limit also -> record duration
			if (pastVal == 1) {
				durationTime++;  // count the duration
			}
			// If at last time step, the value was at the limit -> record duration
			else if (pastVal == 0) {
				durationTime++;
				pastVal = 1;
			}
			// If at last time step, the value was within the limit -> record duration
			else {
				count++; // Went across the limit line
				durationTime += 0.5; // Assume that the duration above the limit between the last and current time step is half time interval
				pastVal = 1;
			}
		}
		else if (array[i] == limitVal) {
			// If at last time step, the value was out of the limit -> record the duration time
			if (pastVal == 1) {
				durationTime++;  // count the duration
				count++;
				pastVal = 0;
			}
			// If at last time step, the value was at the limit also -> does not count anything
			else if (pastVal == 0) {
				// Do nothing
			}
			// If at last time step, the value was within the limit -> count
			else {
				count++; // Went across the limit line
				pastVal = 0;
			}
		}
		else {
			// If at last time step, the value was out of the limit -> record the itersection and the duration time
			if (pastVal == 1) {
				durationTime += 0.5;  //Assume that the duration above the limit between the last and current time step is half time interval
				count++;
				pastVal = -1;
			}
			// If at last time step, the value was at the limit -> do nothing
			else if (pastVal == 0) {
				pastVal = -1;
			}
			// If at last time step, the value was within the limit also -> do nothing
			else {
				// Do nothing
			}
		}
	}

	// Check the voltage value at the end of last metrics collector interval
	if (lastVol >= 0) {
		if ((lastVol < limitVal && array[0] > limitVal) || (lastVol > limitVal && array[0] < limitVal)){
			count++;
			durationTime += 0.5;
		}
		else if (lastVol > limitVal && array[0] > limitVal) {
			durationTime++;  // add the duration without count
		}
		else if (array[0] == limitVal && lastVol != limitVal) {
			durationTime++;  // count the duration
			count++;
		}
	}

	// Update the time steps array with the count size
	if (checkAbove) {
		result.countViolation = count;
		result.durationViolation = durationTime;
	}
	else {
		result.countViolation = count;
		result.durationViolation = (length) - durationTime;
	}

	return result;
}

EXPORT int create_metrics_collector(OBJECT **obj, OBJECT *parent){
	int rv = 0;
	try {
		*obj = gl_create_object(metrics_collector::oclass);
		if(*obj != NULL){
			metrics_collector *my = OBJECTDATA(*obj, metrics_collector);
			gl_set_parent(*obj, parent);
			rv = my->create();
		}
	}
	catch (char *msg){
		gl_error("create_metrics_collector: %s", msg);
	}
	catch (const char *msg){
		gl_error("create_metrics_collector: %s", msg);
	}
	catch (...){
		gl_error("create_metrics_collector: unexpected exception caught");
	}
	return rv;
}

EXPORT int init_metrics_collector(OBJECT *obj){
	metrics_collector *my = OBJECTDATA(obj, metrics_collector);
	int rv = 0;
	try {
		rv = my->init(obj->parent);
	}
	catch (char *msg){
		gl_error("init_metrics_collector: %s", msg);
	}
	catch (const char *msg){
		gl_error("init_metrics_collector: %s", msg);
	}
	return rv;
}

EXPORT TIMESTAMP sync_metrics_collector(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass){
	metrics_collector *my = OBJECTDATA(obj, metrics_collector);
	TIMESTAMP rv = 0;
	try {
		switch(pass){
			case PC_PRETOPDOWN:
				rv = TS_NEVER;
				break;
			case PC_BOTTOMUP:
				rv = TS_NEVER;
				break;
			case PC_POSTTOPDOWN:
				rv = my->postsync(obj->clock, t0);
				obj->clock = t0;
				break;
			default:
				throw "invalid pass request";
		}
	}
	catch(char *msg){
		gl_error("sync_metrics_collector: %s", msg);
	}
	catch(const char *msg){
		gl_error("sync_metrics_collector: %s", msg);
	}
	return rv;
}

EXPORT int commit_metrics_collector(OBJECT *obj){
	int rv = 0;
	metrics_collector *my = OBJECTDATA(obj, metrics_collector);
	try {
		rv = my->commit(obj->clock);
	}
	catch (char *msg){
		gl_error("commit_metrics_collector: %s", msg);
	}
	catch (const char *msg){
		gl_error("commit_metrics_collector: %s", msg);
	}
	return rv;
}

EXPORT int isa_metrics_collector(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj, metrics_collector)->isa(classname);
}

// EOF








