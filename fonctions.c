#include "fonctions.h"

int write_in_queue(RT_QUEUE *msgQueue, void * data, int size);
DJpegimage *take_picture();




void envoyer(void * arg) {
    DMessage *msg;
    int err;

    while (1) {
        rt_printf("tenvoyer : Attente d'un message\n");
        if ((err = rt_queue_read(&queueMsgGUI, &msg, sizeof (DMessage), TM_INFINITE)) >= 0) {
            rt_printf("tenvoyer : envoi d'un message au moniteur\n");
            serveur->send(serveur, msg);
            msg->free(msg);
        } else {
            rt_printf("Error msg queue write: %s\n", strerror(-err));
        }
    }
}


void connecter(void * arg) {
    int status;
    DMessage *message;

    rt_printf("tconnect : Debut de l'exécution de tconnect\n");

    while (1) {
        rt_printf("tconnect : Attente du sémarphore semConnecterRobot\n");

        rt_sem_p(&semConnecterRobot, TM_INFINITE);
        rt_printf("tconnect : Ouverture de la communication avec le robot\n");
        rt_mutex_acquire(&mutexRobot, TM_INFINITE);
        status = robot->open_device(robot);
        rt_mutex_release(&mutexRobot);

        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        etatCommRobot = status;
        rt_mutex_release(&mutexEtat);

        if (status == STATUS_OK) {
            rt_mutex_acquire(&mutexRobot, TM_INFINITE);
            status = robot->start_insecurely(robot);
            rt_mutex_release(&mutexRobot);
		      //status = robot->start(robot);
            if (status == STATUS_OK){
                rt_printf("tconnect : Robot démarrer\n");
            }
        }

        message = d_new_message();
        message->put_state(message, status);

        rt_printf("tconnecter : Envoi message\n");
        message->print(message, 100);
        rt_mutex_acquire(&mutexMove, TM_INFINITE);                    
        if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
            message->free(message);
        }
        rt_mutex_release(&mutexMove);
    }
}

void communiquer(void *arg) {
    DMessage *msg = d_new_message();
    int var1 = 1;
    int num_msg = 0;

    rt_printf("tserver : Début de l'exécution de serveur\n");
    serveur->open(serveur, "8000");
    rt_printf("tserver : Connexion\n");

    rt_mutex_acquire(&mutexEtat, TM_INFINITE);
    etatCommMoniteur = 0;
    rt_mutex_release(&mutexEtat);

    while (var1 > 0) {
        rt_printf("tserver : Attente d'un message\n");
        var1 = serveur->receive(serveur, msg);
        num_msg++;
        if (var1 > 0) {
            switch (msg->get_type(msg)) {
                case MESSAGE_TYPE_ACTION:
                    rt_printf("tserver : Le message %d reçu est une action\n",
                            num_msg);
                    DAction *action = d_new_action();
                    action->from_message(action, msg);
                    switch
                     (action->get_order(action)) {
                        case ACTION_CONNECT_ROBOT:
                            rt_printf("tserver : Action connecter robot\n");
                            rt_sem_v(&semConnecterRobot);
                            break;
                    }
                    break;
                case MESSAGE_TYPE_MOVEMENT:
                    rt_printf("tserver : Le message reçu %d est un mouvement\n",
                            num_msg);
                    rt_mutex_acquire(&mutexMove, TM_INFINITE);
                    move->from_message(move, msg);
                    move->print(move);
                    rt_mutex_release(&mutexMove);
                    break;
            }
        }
    }
}

void watchdog(void *arg){
   int robot_status = 1;
 
	
   rt_printf("twatchdog : Debut de l'éxecution de periodique à 1s\n");
   rt_task_set_periodic(NULL, TM_NOW, 1000000000);

   while(1){
	
     rt_task_wait_period(NULL);
     rt_printf("twatchdog : Activation périodique\n");
    
     rt_mutex_acquire(&mutexEtat, TM_INFINITE);
     robot_status = etatCommRobot;
     rt_mutex_release(&mutexEtat);
    
     if (robot_status == STATUS_OK) {
       // Si on est à 50ms près de de l'expiration
       rt_mutex_acquire(&mutexRobot, TM_INFINITE);
       robot_status = d_robot_reload_wdt(robot);
       rt_mutex_release(&mutexRobot);

     }
    
   }

}


void batterie(void *arg){
    int battery_level;
    int robot_status = 1;
    DMessage *message;
    DBattery *battery = d_new_battery();
    int attempt = 0;

    rt_printf("tbatterie : Debut de l'éxecution de periodique à 250ms\n");
    rt_task_set_periodic(NULL, TM_NOW, 250000000);

    while(attempt <= 10){
        /* Attente de l'activation périodique */
        rt_task_wait_period(NULL);
        rt_printf("tbatterie : Activation périodique\n");

        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        robot_status = etatCommRobot;
        rt_mutex_release(&mutexEtat);

        if (robot_status == STATUS_OK) {
            printf("OK : %d\n", attempt);
            rt_mutex_acquire(&mutexRobot, TM_INFINITE);
            robot_status = d_robot_get_vbat(robot, &battery_level);
            rt_mutex_release(&mutexRobot);
            printf("batt lvl : %d | status : %d\n", battery_level, robot_status);
            if (robot_status != STATUS_OK){
                attempt++;
            }
            else{
                attempt = 0;
                message = d_new_message();
                d_battery_set_level(battery, battery_level);
                d_message_put_battery_level(message, battery);
                rt_printf("tbatterie : Envoi message\n");
                rt_mutex_acquire(&mutexCom, TM_INFINITE);
                if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                    message->free(message);
                }
                rt_mutex_release(&mutexCom);

            }

        }

    }
}


DJpegimage *take_picture(){
   DImage * image = d_new_image();
   camera_v->get_frame(camera_v,image);
   DJpegimage *jpeg=d_new_jpegimage();
   if(image!=NULL){
      jpeg->compress(jpeg,image);
      image->free(image);
      return jpeg;
   }


   return NULL;
}


void camera(void *arg){
   int moniteur_status = 1;
 
	
   rt_printf("tcamera : Debut de l'éxecution de periodique à 600ms\n");
   rt_task_set_periodic(NULL, TM_NOW, 600000000);
   camera_v->open(camera_v);

   while(1){
     	rt_task_wait_period(NULL);
	rt_printf("tcamera : Activation périodique\n");
	rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        moniteur_status = etatCommRobot;
        rt_mutex_release(&mutexEtat);
	
	if(moniteur_status==STATUS_OK){
		DJpegimage *jpg=take_picture();
		DMessage* message=d_new_message();
		message->put_jpeg_image(message,jpg);
		rt_mutex_acquire(&mutexCom, TM_INFINITE);
                if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                    message->free(message);
                }
                rt_mutex_release(&mutexCom);
	}
     
     
   }

}


void deplacer(void *arg) {
    int status = 1;
    int gauche;
    int droite;
    int attempt = 0;
    DMessage *message;

    rt_printf("tmove : Debut de l'éxecution de periodique à 1s\n");
    rt_task_set_periodic(NULL, TM_NOW, 1000000000);

    while (1) {
        /* Attente de l'activation périodique */
        rt_task_wait_period(NULL);
        rt_printf("tmove : Activation périodique\n");

        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        status = etatCommRobot;
        rt_mutex_release(&mutexEtat);

        if (status == STATUS_OK) {
            rt_mutex_acquire(&mutexMove, TM_INFINITE);
            switch (move->get_direction(move)) {
                case DIRECTION_FORWARD:
                    gauche = MOTEUR_ARRIERE_LENT;
                    droite = MOTEUR_ARRIERE_LENT;
                    break;
                case DIRECTION_LEFT:
                    gauche = MOTEUR_ARRIERE_LENT;
                    droite = MOTEUR_AVANT_LENT;
                    break;
                case DIRECTION_RIGHT:
                    gauche = MOTEUR_AVANT_LENT;
                    droite = MOTEUR_ARRIERE_LENT;
                    break;
                case DIRECTION_STOP:
                    gauche = MOTEUR_STOP;
                    droite = MOTEUR_STOP;
                    break;
                case DIRECTION_STRAIGHT:
                    gauche = MOTEUR_AVANT_LENT;
                    droite = MOTEUR_AVANT_LENT;
                    break;
            }
            rt_mutex_release(&mutexMove);

            rt_mutex_acquire(&mutexRobot, TM_INFINITE);
            status = robot->set_motors(robot, gauche, droite);
            rt_mutex_release(&mutexRobot);

            if ((status != STATUS_OK)){
                attempt++;
            }else{
                attempt = 0;
            }

            if ((status != STATUS_OK) && attempt >= 3) {
                rt_mutex_acquire(&mutexEtat, TM_INFINITE);
                etatCommRobot = status;
                rt_mutex_release(&mutexEtat);

                message = d_new_message();
                message->put_state(message, status);

                rt_printf("tmove : Envoi message\n");
                rt_mutex_acquire(&mutexCom, TM_INFINITE);
                if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                    message->free(message);
                }
                rt_mutex_release(&mutexCom);
            }
        }
    }
}

int write_in_queue(RT_QUEUE *msgQueue, void * data, int size) {
    void *msg;
    int err;

    msg = rt_queue_alloc(msgQueue, size);
    memcpy(msg, &data, size);

    if ((err = rt_queue_send(msgQueue, msg, sizeof (DMessage), Q_NORMAL)) < 0) {
        rt_printf("Error msg queue send: %s\n", strerror(-err));
    }
    rt_queue_free(&queueMsgGUI, msg);

    return err;
}
