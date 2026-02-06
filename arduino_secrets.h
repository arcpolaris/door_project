#pragma once

#define PHONE 0
#define PINET 1
#define HOME 2
#define SCHOOL 3

#define NETWORK PINET

#if NETWORK == PHONE
#define SECRET_SSID "iBirb"
#define SECRET_PASS "kingisaverygoodbird"
#elif NETWORK == PINET
#define SECRET_SSID "pi-2_4"
#define SECRET_PASS "R@spberryP!3"
#elif NETWORK == HOME
#define SECRET_SSID "Jiggety"
#define SECRET_PASS "6e4HfyL*rdF:Dfn$"
#elif NETWORK == SCHOOL
#define SECRET_SSID "EPS-Events"
#define SECRET_PASS "GoEagles2024!"
#else
#error no network specified
#endif