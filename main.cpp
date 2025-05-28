/**
 * @file main.cpp
 * @author Simen Skretteberg Hanisch, Daniel Lindestad, Ryan Kevique Ngabo Wanje
 */

#include "mbed.h"

/* === Include === */
#include <upside_downside_communication/up_down_comm.hpp>

Thread thread_UpDownComm;

int main()
{
    thread_UpDownComm.start(ThreadFunc_HTTP_GET);
}