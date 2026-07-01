#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <uacpi/event.h>
#include <uacpi/utilities.h>
#include <uacpi/sleep.h>
#include "acpi.h"
#include "../io/log.h"
#include "../utilities/vector.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../utilities/util.h"
#include "../tables/interrupts.h"
#include "apic.h"

extern vector_t shared_pages_vec; // don't want to create a header just for this


void acpi_shutdown(){

    enable_interrupts();

    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)){
        logf("Failed to prepare sleep: %s",uacpi_status_to_string(ret));
        panic("");
    }

    disable_interrupts();

    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)){
        logf("Failed to prepare sleep: %s",uacpi_status_to_string(ret));
        panic("");
    }

    panic("Failed to shut down system");
}

void init_acpi(){
    init_vector(&shared_pages_vec); // needed for uacpi memory mapping stuff
    
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_initialize: %s",uacpi_status_to_string(ret));
        panic("");
    }
    
    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_namespace_load: %s",uacpi_status_to_string(ret));
        panic("");
    }

    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_namespace_initialize: %s",uacpi_status_to_string(ret));
        panic("");
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)){
        errorf("uacpi_finalize_gpe_initialization: %s",uacpi_status_to_string(ret));
        panic("");
    }

    initialize_apics();
}