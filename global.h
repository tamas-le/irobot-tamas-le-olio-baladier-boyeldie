/* 
 * File:   global.h
 * Author: pehladik
 *
 * Created on 12 janvier 2012, 10:11
 */

#ifndef GLOBAL_H
#define	GLOBAL_H

#include "includes.h"

/* @descripteurs des tâches */
extern RT_TASK tServeur;
extern RT_TASK tconnect;
extern RT_TASK tmove;
extern RT_TASK tenvoyer;
extern RT_TASK tbatterie;
extern RT_TASK twatchdog;
extern RT_TASK tcamera;

/* @descripteurs des mutex */
extern RT_MUTEX mutexEtat;
extern RT_MUTEX mutexMove;
extern RT_MUTEX mutexCom;
extern RT_MUTEX mutexRobot;
extern RT_MUTEX mutexCamera;
extern RT_MUTEX mutexArene;
extern RT_MUTEX mutexPosition;
extern RT_MUTEX mutexMission;
extern RT_MUTEX mutexAttempt;

/* @descripteurs des sempahore */
extern RT_SEM semConnecterRobot;

/* @descripteurs des files de messages */
extern RT_QUEUE queueMsgGUI;

/* @variables partagées */
extern int etatCommMoniteur;
extern int etatCommRobot;
extern int etatCamera;
extern int attempt;
extern DServer *serveur;
extern DRobot *robot;
extern DMovement *move;
extern DCamera * camera_v;
extern DArena * arene;
extern DPosition *position;
extern DMission *mission;


/* @constantes */
extern int MSG_QUEUE_SIZE;
extern int PRIORITY_TSERVEUR;
extern int PRIORITY_TCONNECT;
extern int PRIORITY_TMOVE;
extern int PRIORITY_TENVOYER;
extern int PRIORITY_TBATTERIE;
extern int PRIORITY_TWATCHDOG;
extern int PRIORITY_TCAMERA;

#endif	/* GLOBAL_H */

