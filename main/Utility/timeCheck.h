
/******************************************************************************
*
* Embedded software team.
* (c) Copyright 2018.
* ALL RIGHTS RESERVED.
*
***************************************************************************/
/**
 *
 * @file         timeCheck.h
 *
 * @author    	quanvu
 *
 * @version   1.0
 *
 * @date
 *
 * @brief     Brief description of the file
 *
 * Detailed Description of the file. If not used, remove the separator above.
 *
 */

#ifndef APPS_TIMER_TIMECHECK_H_
#define APPS_TIMER_TIMECHECK_H_


/******************************************************************************
* Includes
******************************************************************************/


#include "Global.h"


/******************************************************************************
* Constants
******************************************************************************/



/******************************************************************************
* Macros
******************************************************************************/



/******************************************************************************
* Types
******************************************************************************/


/**
 * @brief Use brief, otherwise the index won't have a brief explanation.
 *
 * Detailed explanation.
 */




/******************************************************************************
* Global variables
******************************************************************************/


/******************************************************************************
* Global functions
******************************************************************************/
uint32_t elapsedTime(uint32_t newTime,uint32_t oldTime);
bool timeIsAfter(uint32_t newTime,uint32_t oldTime);


/******************************************************************************
* Inline functions
******************************************************************************/





#endif /* APPS_TIMER_TIMECHECK_H_ */
