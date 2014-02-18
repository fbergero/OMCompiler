/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-CurrentYear, Linköping University,
 * Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF GPL VERSION 3
 * AND THIS OSMC PUBLIC LICENSE (OSMC-PL).
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES RECIPIENT'S
 * ACCEPTANCE OF THE OSMC PUBLIC LICENSE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from Linköping University, either from the above address,
 * from the URLs: http://www.ida.liu.se/projects/OpenModelica or
 * http://www.openmodelica.org, and in the OpenModelica distribution.
 * GNU version 3 is obtained from: http://www.gnu.org/copyleft/gpl.html.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS
 * OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */

#include "solver_main.h"
#include "events.h"
#include "dassl.h"

#include "simulation/simulation_runtime.h"
#include "simulation/results/simulation_result.h"
#include "openmodelica_func.h"
#include "linearSystem.h"
#include "nonlinearSystem.h"
#include "mixedSystem.h"

#include "util/omc_error.h"
#include "simulation/options.h"
#include <math.h>
#include <string.h>
#include <errno.h>
#include <float.h>



/*! \fn updateContinuousSystem
 *
 *  Function to update the whole system with EventIteration.
 *  Evaluate the functionDAE()
 *
 *  \param [ref] [data]
 */
void updateContinuousSystem(DATA *data)
{
  data->callback->input_function(data);
  data->callback->functionODE(data);
  data->callback->functionAlgebraics(data);
  data->callback->output_function(data);
  data->callback->function_storeDelayed(data);
  storePreValues(data);
}



/*! \fn performSimulation(DATA* data, SOLVER_INFO* solverInfo)
 *
 *  \param [ref] [data]
 *  \param [ref] [solverInfo]
 *
 *  This function performs the simulation controlled by solverInfo.
 */
int prefixedName_performSimulation(DATA* data, SOLVER_INFO* solverInfo)
{

  int retValIntegrator=0;
  int retValue=0;
  int i, ui, eventType, retry=0;

  FILE *fmt = NULL;
  unsigned int stepNo=0;

  SIMULATION_INFO *simInfo = &(data->simulationInfo);

  solverInfo->currentTime = simInfo->startTime;
  
  unsigned int __currStepNo = 0;

  if(measure_time_flag)
  {
    size_t len = strlen(data->modelData.modelFilePrefix);
    char* filename = (char*) malloc((len+11) * sizeof(char));
    strncpy(filename,data->modelData.modelFilePrefix,len);
    strncpy(&filename[len],"_prof.data",11);
    fmt = fopen(filename, "wb");
    if(!fmt)
    {
      warningStreamPrint(LOG_SOLVER, 0, "Time measurements output file %s could not be opened: %s", filename, strerror(errno));
      fclose(fmt);
      fmt = NULL;
    }
    free(filename);
  }

  printAllVarsDebug(data, 0, LOG_DEBUG); /* ??? */

  /***** Start main simulation loop *****/
  while(solverInfo->currentTime < simInfo->stopTime)
  {
    omc_alloc_interface.collect_a_little();

    currectJumpState = ERROR_SIMULATION;
    /* try */
    if(!setjmp(simulationJmpbuf))
    {
      if(measure_time_flag)
      {
        for(i=0; i<data->modelData.modelDataXml.nFunctions + data->modelData.modelDataXml.nProfileBlocks; i++)
        {
          rt_clear(i + SIM_TIMER_FIRST_FUNCTION);
        }
        rt_clear(SIM_TIMER_STEP);
        rt_tick(SIM_TIMER_STEP);
      }

      rotateRingBuffer(data->simulationData, 1, (void**) data->localData);

      /***** Calculation next step size *****/
      if(solverInfo->didEventStep == 1)
      {
        infoStreamPrint(LOG_SOLVER, 0, "offset value for the next step: %.10f", (solverInfo->currentTime - solverInfo->laststep));
      }
      else
      {
        __currStepNo++;
      }
      solverInfo->currentStepSize = (double)(__currStepNo*(simInfo->stopTime-simInfo->startTime))/(simInfo->numSteps) + simInfo->startTime - solverInfo->currentTime;

      // if retry reduce stepsize 
      if(retry)
      {
        solverInfo->currentStepSize /= 2;
      }
      /***** End calculation next step size *****/



      /* check for next time event */
      checkForSampleEvent(data, solverInfo);
      infoStreamPrint(LOG_SOLVER, 1, "call solver from %g to %g (stepSize: %g)", solverInfo->currentTime, solverInfo->currentTime + solverInfo->currentStepSize, solverInfo->currentStepSize);

      /*
       * integration step determine all states by a integration method
       * update continuous system
       */
      communicateStatus("Running", (solverInfo->currentTime-simInfo->startTime)/(simInfo->stopTime-simInfo->startTime));
      retValIntegrator = solver_main_step(data, solverInfo);  

      if (S_OPTIMIZATION == solverInfo->solverMethod) break;

      updateContinuousSystem(data);

      saveZeroCrossings(data);
      if (ACTIVE_STREAM(LOG_SOLVER)) messageClose(LOG_SOLVER);


      /***** Event handling *****/
      if (measure_time_flag) rt_tick(SIM_TIMER_EVENT);
      
      eventType = checkEvents(data, solverInfo->eventLst, &(solverInfo->currentTime), solverInfo);
      if(eventType > 0) /* event */
      {
        currectJumpState = ERROR_EVENTSEARCH;
        infoStreamPrint(LOG_EVENTS, 1, "%s event at time %.12g", eventType == 1 ? "time" : "state", solverInfo->currentTime);
        /* prevent emit if noEventEmit flag is used */
        if (!(omc_flag[FLAG_NOEVENTEMIT])) /* output left limit */
          sim_result.emit(&sim_result,data);
        handleEvents(data, solverInfo->eventLst, &(solverInfo->currentTime), solverInfo);
        if (ACTIVE_STREAM(LOG_EVENTS)) messageClose(LOG_EVENTS);
        currectJumpState = ERROR_SIMULATION;

        solverInfo->didEventStep = 1;
        overwriteOldSimulationData(data);
      }
      else /* no event */
      {
        solverInfo->laststep = solverInfo->currentTime;
        solverInfo->didEventStep=0;
      }
      
      if (measure_time_flag) rt_accumulate(SIM_TIMER_EVENT);
      /***** End event handling *****/


      /***** check state selection *****/
      if (stateSelection(data, 1, 1))
      {
        /* if new set is calculated reinit the solver */
        solverInfo->didEventStep = 1;
        overwriteOldSimulationData(data);
      }

      /* Check for warning of variables out of range assert(min<x || x>xmax, ...)*/
      data->callback->checkForAsserts(data);

      if(retry)
      {
        retry=0;
      }
      
      /***** Emit this time step *****/
      storePreValues(data);
      storeOldValues(data);
      saveZeroCrossings(data);

      if (fmt)
      {
        int flag = 1;
        double tmpdbl;
        unsigned int tmpint;
        rt_tick(SIM_TIMER_OVERHEAD);
        rt_accumulate(SIM_TIMER_STEP);
        
        /* Disable time measurements if we have trouble writing to the file... */
        flag = flag && 1 == fwrite(&stepNo, sizeof(unsigned int), 1, fmt);
        stepNo++;
        flag = flag && 1 == fwrite(&(data->localData[0]->timeValue), sizeof(double), 1, fmt);
        tmpdbl = rt_accumulated(SIM_TIMER_STEP);
        flag = flag && 1 == fwrite(&tmpdbl, sizeof(double), 1, fmt);
        for(i=0; i<data->modelData.modelDataXml.nFunctions + data->modelData.modelDataXml.nProfileBlocks; i++)
        {
          tmpint = rt_ncall(i + SIM_TIMER_FIRST_FUNCTION);
          flag = flag && 1 == fwrite(&tmpint, sizeof(unsigned int), 1, fmt);
        }
        for(i=0; i<data->modelData.modelDataXml.nFunctions + data->modelData.modelDataXml.nProfileBlocks; i++)
        {
          tmpdbl = rt_accumulated(i + SIM_TIMER_FIRST_FUNCTION);
          flag = flag && 1 == fwrite(&tmpdbl, sizeof(double), 1, fmt);
        }
        rt_accumulate(SIM_TIMER_OVERHEAD);
        if (!flag)
        {
          warningStreamPrint(LOG_SOLVER, 0, "Disabled time measurements because the output file could not be generated: %s", strerror(errno));
          fclose(fmt);
          fmt = NULL;
        }
      }
      
      /* prevent emit if noEventEmit flag is used, if it's an event */
      if ((omc_flag[FLAG_NOEVENTEMIT] && solverInfo->didEventStep == 0) || !omc_flag[FLAG_NOEVENTEMIT])
      {
        sim_result.emit(&sim_result, data);
      }

      printAllVarsDebug(data, 0, LOG_DEBUG);  /* ??? */
      /***** end of Emit this time step *****/

      /* save dassl stats before reset */
      if (solverInfo->didEventStep == 1 && solverInfo->solverMethod == 3)
      {
        for(ui=0; ui<numStatistics; ui++)
        {
          ((DASSL_DATA*)solverInfo->solverData)->dasslStatistics[ui] += ((DASSL_DATA*)solverInfo->solverData)->dasslStatisticsTmp[ui];
        }
      }

      /* Check if terminate()=true */
      if(terminationTerminate)
      {
        printInfo(stdout, TermInfo);
        fputc('\n', stdout);
        infoStreamPrint(LOG_STDOUT, 0, "Simulation call terminate() at time %f\nMessage : %s", data->localData[0]->timeValue, TermMsg);
        simInfo->stopTime = solverInfo->currentTime;
      }

      /* terminate for some cases:
       * - integrator fails
       * - non-linear system failed to solve
       * - assert was called
       */
      if (retValIntegrator != 0
       || check_nonlinear_solutions(data, 0)
       || check_linear_solutions(data, 0)
       || check_mixed_solutions(data, 0))
      {
        if(retValIntegrator)
        {
          retValue = -1 + retValIntegrator;
          infoStreamPrint(LOG_STDOUT, 0, "model terminate | Integrator failed. | Simulation terminated at time %g", solverInfo->currentTime);
        }
        else if(check_nonlinear_solutions(data, 0))
        {
          retValue = -2;
          infoStreamPrint(LOG_STDOUT, 0, "model terminate | non-linear system solver failed. | Simulation terminated at time %g", solverInfo->currentTime);
        }
        else if(check_linear_solutions(data, 0))
        {
          retValue = -3;
          infoStreamPrint(LOG_STDOUT, 0, "model terminate | linear system solver failed. | Simulation terminated at time %g", solverInfo->currentTime);
        }
        else if(check_mixed_solutions(data, 0))
        {
          retValue = -3;
          infoStreamPrint(LOG_STDOUT, 0, "model terminate | mixed system solver failed. | Simulation terminated at time %g", solverInfo->currentTime);
        }
        else
        {
          retValue = -1;
          infoStreamPrint(LOG_STDOUT, 0, "model terminate | probably a strong component solver failed. For more information use flags -lv LOG_NLS, LOG_LS. | Simulation terminated at time %g", solverInfo->currentTime);
        }
        break;
      }
    }
    else
    { /* catch */
      if(!retry)
      {
        /* reduce step size by a half and try again */
        solverInfo->laststep = solverInfo->currentTime - solverInfo->laststep;

        /* restore old values and try another step with smaller step-size by dassl*/
        restoreOldValues(data);
        solverInfo->currentTime = data->localData[0]->timeValue;
        overwriteOldSimulationData(data);
        warningStreamPrint(LOG_STDOUT, 0, "Integrator attempt to handle a problem with a called assert.");
        retry = 1;
        solverInfo->didEventStep = 1;
      } else {
        retValue =  -1;
        infoStreamPrint(LOG_STDOUT, 0, "model terminate | Simulation terminated by an assert at time: %g", data->localData[0]->timeValue);
        break;
      }
    }
  } /* end while solver */

  if(fmt) fclose(fmt);

  return retValue;
}
