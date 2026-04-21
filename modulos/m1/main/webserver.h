#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "controller.h"

void start_webserver(Controller *controller);
void webserver_task(void *pvParameters);

#endif
