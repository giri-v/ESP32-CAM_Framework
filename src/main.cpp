////////////////////////////////////////////////////////////////////
/// @file main.cpp
/// @brief Application Entry Point and Main Functions
////////////////////////////////////////////////////////////////////

#ifndef BUILD_OPTIONS_H
#include "build_options.h"
#endif

#ifndef APP_CONFIG_H
#include "app_config.h"
#endif

#ifndef FRAMEWORK_H
#include "framework.h"
#endif

// ************ Customizeable Functions *************
#include "app_functions.h"

// ********** Function Declarations **********
#include "framework_functions.h"

void setup()
{
  String oldMethodName = methodName;
  methodName = "setup()";

  framework_setup();

  app_setup();

  framework_start();

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void loop()
{
  framework_loop();
  
  app_loop();

  if (isFirstLoop)
    isFirstLoop = false;
}