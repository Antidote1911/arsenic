/**
 * scrypt-lite
 * progressbar.h
 *
 * @autor Leon Latsch
 * @version 1.1
 */

/*
    MIT License

    Copyright (c) 2019 Leon Latsch
*/

#pragma once

#include <chrono>
#include <cstdio>
#include <thread>

/**
 * Print the percentage if a progress bar
 */
void printProgress(double percentage);
