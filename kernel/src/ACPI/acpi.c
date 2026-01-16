#include <uacpi/acpi.h>
#include <uacpi/event.h>
#include <uacpi/utilities.h>
#include "../io/log.h"

void init_acpi(){
    log("here");
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_initialize: %s",uacpi_status_to_string(ret));
        panic("");
    }
    log("initialized 0");
    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_namespace_load: %s",uacpi_status_to_string(ret));
        panic("");
    }
    log("loaded namespace");
    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_PIC);

    log ("loaded interrupt model");
    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_namespace_initialize: %s",uacpi_status_to_string(ret));
        panic("");
    }
    log("initialized namespace");

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_finalize_gpe_initialization: %s",uacpi_status_to_string(ret));
        panic("");
    }
    log("finalized gpe init");

}