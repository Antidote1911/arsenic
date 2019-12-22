/**
 * scrypt-lite
 * progressbar.cpp
 * 
 * @autor Leon Latsch
 * @version 1.1
 */

/*
    MIT License

    Copyright (c) 2019 Leon Latsch
*/

#define PBSTR "##################################################"
#define PBWIDTH 50

#include <cstdio>
#include <thread>
#include <chrono>

void printProgress (double percentage) {
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf ("\r[\033[1;34mLOAD\033[0m]%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush (stdout);
}
