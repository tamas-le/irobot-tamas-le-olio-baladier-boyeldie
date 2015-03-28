/*
 * File:   global.h
 * Author: pehladik
 *
 * Created on 21 avril 2011, 12:14
 */

#include "global.h"

RT_TASK tServeur;
RT_TASK tconnect;
RT_TASK tmove;
RT_TASK tenvoyer;
RT_TASK tbatterie;
RT_TASK twatchdog;
RT_TASK tcamera;

RT_MUTEX mutexEtat;
RT_MUTEX mutexMove;
RT_MUTEX mutexCom;
RT_MUTEX mutexRobot;
RT_MUTEX mutexCamera;
RT_MUTEX mutexArene;
RT_MUTEX mutexPosition;
RT_MUTEX mutexMission;

RT_SEM semConnecterRobot;

RT_QUEUE queueMsgGUI;

int etatCommMoniteur = 1;
int etatCommRobot = 1;
int etatCamera=ACTION_STOP_COMPUTE_POSITION;

DRobot *robot;
DMovement *move;
DServer *serveur;
DCamera * camera_v;
DArena * arene;
DPosition *position;
DMission *mission;


int MSG_QUEUE_SIZE = 10;

int PRIORITY_TSERVEUR = 30;
int PRIORITY_TCONNECT = 20;
int PRIORITY_TMOVE = 10;
int PRIORITY_TENVOYER = 25;
int PRIORITY_TBATTERIE = 5;
int PRIORITY_TWATCHDOG = 30;
int PRIORITY_TCAMERA =15;
