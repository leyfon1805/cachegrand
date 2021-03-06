#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include "misc.h"
#include "log.h"
#include "fatal.h"
#include "signals_support.h"

LOG_PRODUCER_CREATE_DEFAULT("signal_handler/sigsegv", signal_handler_sigsegv)

char *unknown_signal_name = "UNKNOWN_SIGNAL_NAME";

void signals_support_handler_sigsegv_fatal(int signal_number) {
    char *signal_name = SIGNALS_SUPPORT_NAME_WRAPPER(signal_number);
    FATAL(
            LOG_PRODUCER_DEFAULT,
            "Recived segmentation fault signal (%s %d):\n",
            signal_name,
            signal_number);
}

void signals_support_register_sigsegv_fatal_handler() {
    signals_support_register_signal_handler(
            SIGSEGV,
            signals_support_handler_sigsegv_fatal,
            NULL);
}

char* signals_support_name(
        int signal_number,
        char* buffer,
        size_t buffer_size) {
    if (signal_number >= NSIG || buffer_size <= 1) {
        return NULL;
    }

    char* signal_name = strsignal(signal_number);
    strncpy(buffer, signal_name, buffer_size - 1);
    buffer[buffer_size - 1] = 0;

    return buffer;
}

bool signals_support_register_signal_handler(
        int signal_number,
        __sighandler_t signal_handler,
        struct sigaction* previous_action) {
    char *signal_name;
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = signal_handler;
    action.sa_flags = 0;

    if (signal_number >= NSIG) {
        return false;
    }

    signal_name = SIGNALS_SUPPORT_NAME_WRAPPER(signal_number);

    LOG_D(
            LOG_PRODUCER_DEFAULT,
            "Registering signal handler <%s (%d)>",
            signal_name,
            signal_number);

    if (sigaction(signal_number, &action, previous_action) < 0) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Unable to set the handler for <%s (%d)>",
                signal_name,
                signal_number);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);

        return false;
    }

    return true;
}
