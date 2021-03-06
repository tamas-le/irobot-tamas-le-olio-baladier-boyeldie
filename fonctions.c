#include "fonctions.h"
#include <math.h>

int write_in_queue(RT_QUEUE *msgQueue, void * data, int size);
DJpegimage *take_picture();


void reset(){
  rt_printf("Communication error entre le moniteur et le superviseur est survenue.\n");
  rt_mutex_acquire(&mutexEtat, TM_INFINITE);
    etatCommMoniteur = 1;
  rt_mutex_release(&mutexEtat);

  serveur->close(serveur);

  rt_task_delete(&tServeur);
  rt_task_delete(&tconnect);
  rt_task_delete(&tmove);
  rt_task_delete(&tcamera);
  rt_task_delete(&tbatterie);
  rt_task_delete(&tenvoyer);

  rt_sem_delete(&semConnecterRobot);

  rt_mutex_delete(&mutexEtat);
  rt_mutex_delete(&mutexMove);
  rt_mutex_delete(&mutexCom);
  rt_mutex_delete(&mutexRobot);
  rt_mutex_delete(&mutexCamera);
  rt_mutex_delete(&mutexArene);
  rt_mutex_delete(&mutexPosition);
  rt_mutex_delete(&mutexMission);

  rt_queue_delete(&queueMsgGUI);


  d_robot_free(robot);
  d_movement_free(move);
  d_server_free(serveur);
  d_camera_free(camera_v);
}





//retourne le vrai status du robot
//etat_communication_robot
int checkConnection(int status){
    DMessage *message;
    int err_nb;

    rt_mutex_acquire(&mutex_robot_communication_error, TM_INFINITE);
    err_nb = robot_communication_error;
    rt_mutex_release(&mutex_robot_communication_error);


    if (err_nb <= 3){

      if(status == STATUS_OK){
        rt_mutex_acquire(&mutex_robot_communication_error, TM_INFINITE);
        robot_communication_error = 0;
        rt_mutex_release(&mutex_robot_communication_error);
        rt_printf("CONNECTION ROBOT OK.%d\n", robot_communication_error);
      }else{
        rt_mutex_acquire(&mutex_robot_communication_error, TM_INFINITE);
        robot_communication_error++;
        
        rt_printf("CONNECTION ROBOT PERDU. ATTEMPT N'%d\n", robot_communication_error);
        err_nb = robot_communication_error;
        rt_mutex_release(&mutex_robot_communication_error);
        if (err_nb > 3){

          //on considère qu'on a perdu la connection
          // Envoi d'un message au moniteur
          message = d_new_message();            
          message->put_state(message, status);

          rt_mutex_acquire(&mutexCom, TM_INFINITE);
          while (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
              message->free(message);
          }
          rt_mutex_release(&mutexCom);
          rt_printf("LE ROBOT N'EST PLUS CONNECTE \n");
          rt_mutex_acquire(&mutexRobot, TM_INFINITE);
          robot->close_com(robot);
          rt_mutex_release(&mutexRobot);
          rt_sem_p(&semConnectedRobot, TM_INFINITE);
        }
      }
    }

    return ((robot_communication_error > 3) ? 1 : status);
}








void envoyer(void * arg) {
    DMessage *msg;
    int err;
    int byte;

    while (1) {
        rt_printf("tenvoyer : Attente d'un message\n");
        if ((err = rt_queue_read(&queueMsgGUI, &msg, sizeof (DMessage), TM_INFINITE)) >= 0) {
            rt_printf("tenvoyer : envoi d'un message au moniteur\n");
            byte = serveur->send(serveur, msg);
            msg->free(msg);
            if (byte <= 0){
              //reset();
            }
        } else {
            rt_printf("Error msg queue write: %s\n", strerror(-err));
        }
    }
}


void connecter(void * arg) {
    int status,err;
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

            rt_mutex_acquire(&mutexEtat, TM_INFINITE);
            etatCommRobot = status;
            rt_mutex_release(&mutexEtat);

            if (status == STATUS_OK){
                rt_printf("tconnect : Robot démarrer\n");

                rt_mutex_acquire(&mutex_robot_communication_error, TM_INFINITE);
                robot_communication_error = 0;
                rt_mutex_release(&mutex_robot_communication_error);

                rt_sem_v(&semConnectedRobot);
                rt_sem_v(&semConnectedRobot);

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
    int byte = 1;
    int num_msg = 0;

    rt_printf("tserver : Début de l'exécution de serveur\n");
    serveur->open(serveur, "8000");
    rt_printf("tserver : Connexion\n");
    rt_mutex_acquire(&mutexEtat, TM_INFINITE);
    etatCommMoniteur = 0;
    rt_mutex_release(&mutexEtat);

    while (byte > 0) {
        rt_printf("tserver : Attente d'un message\n");
        byte = serveur->receive(serveur, msg);
        num_msg++;
        if (byte > 0) {
            switch (msg->get_type(msg)) {
                case MESSAGE_TYPE_ACTION:
                    rt_printf("tserver : Le message %d reçu est une action\n",
                            num_msg);
                    DAction *action = d_new_action();
                    action->from_message(action, msg);
                    switch (action->get_order(action)) {
                        case ACTION_CONNECT_ROBOT:
                            rt_printf("tserver : Action connecter robot\n");
                            rt_sem_v(&semConnecterRobot);
                            break;
                  			case ACTION_FIND_ARENA:
                  			    rt_printf("tserver : Action rechercher arène\n");
                  			    rt_mutex_acquire(&mutexCamera, TM_INFINITE);
                                		    etatCamera=ACTION_FIND_ARENA;
                                    	    rt_mutex_release(&mutexCamera);
                  			    break;
                  			case ACTION_ARENA_IS_FOUND:
                  			    rt_printf("tserver : On a trouvé l'arène\n");
                  			    rt_mutex_acquire(&mutexCamera, TM_INFINITE);
                                		    etatCamera=ACTION_ARENA_IS_FOUND;
                                    	    rt_mutex_release(&mutexCamera);
                  			break;
                  			case ACTION_ARENA_FAILED:
                  			    rt_printf("tserver : On a trouvé pas :'( l'arène\n");
                  			    rt_mutex_acquire(&mutexCamera, TM_INFINITE);
                                		    etatCamera=ACTION_ARENA_FAILED;
                                    	    rt_mutex_release(&mutexCamera);
                  			break;
                  			case ACTION_COMPUTE_CONTINUOUSLY_POSITION:
                  			    rt_printf("tserver : On calcul la position du robot\n");
                  			    rt_mutex_acquire(&mutexCamera, TM_INFINITE);
                                		    etatCamera=ACTION_COMPUTE_CONTINUOUSLY_POSITION;
                                    	    rt_mutex_release(&mutexCamera);
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

        }else{
          //reset();
        }
    }
}






float determiner_angle(DPosition *M, DPosition *R){
	double alpha;
	double x;
	double y;
	y=(M->get_y(M))-(R->get_y(R));
	x=(M->get_x(M))-(R->get_x(R));
	alpha = atan2(y,x);
	return alpha;
}

float rotationRobot(DPosition *M, DPosition *R){
	float orientation_robot;	
	orientation_robot = R->get_orientation(R);
	return (orientation_robot + determiner_angle(M,R));
}


void start_mission(void *arg){
	DPosition* positionMission = d_new_position();

	rt_mutex_acquire(&mutexMission, TM_INFINITE);
	mission = d_new_mission();
		
	rt_mutex_acquire(&mutexRobot, TM_INFINITE);
	mission->get_position(mission,positionMission);
	robot->turn(robot, rotationRobot(positionMission,position),1);
	rt_mutex_release(&mutexRobot);	

	rt_mutex_release(&mutexMission);
}


/*
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
*/





void batterie(void *arg){

    rt_sem_p(&semConnectedRobot, TM_INFINITE);

    int battery_level;
    int robot_status = 1;
    DMessage *message;
    DBattery *battery = d_new_battery();

    rt_printf("tbatterie : Debut de l'éxecution de periodique à 250ms\n");
    rt_task_set_periodic(NULL, TM_NOW, 250000000);


    while(1){
        /* Attente de l'activation périodique */
        rt_task_wait_period(NULL);
        rt_printf("tbatterie : Activation périodique\n");
        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        robot_status = etatCommMoniteur;
        rt_mutex_release(&mutexEtat);

        if (robot_status == STATUS_OK){

          rt_mutex_acquire(&mutexRobot, TM_INFINITE);
          robot_status = d_robot_get_vbat(robot, &battery_level);
          rt_mutex_release(&mutexRobot);

          rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        etatCommRobot = robot_status;
        rt_mutex_release(&mutexEtat);
          
          printf("batt lvl : %d | status : %d\n", battery_level, robot_status);

            if (robot_status == STATUS_OK) {
              printf("CONNECTION NON PERDU.\n");
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

        checkConnection(robot_status);

    }
}




DJpegimage* take_picture(int isArena){
   //is Arena vaut 1 si on veut dessiner l'arène 0 sinon.
   DImage * image = d_new_image();
   camera_v->get_frame(camera_v,image);
   DJpegimage *jpeg=d_new_jpegimage();
   if(image!=NULL){
      if (isArena){
		rt_mutex_acquire(&mutexArene, TM_INFINITE);
		arene=image->compute_arena_position(image);
		d_imageshop_draw_arena(image,arene);
		rt_mutex_release(&mutexArene);
	}
      jpeg->compress(jpeg,image);
      image->free(image);
      return jpeg;
   }


   return NULL;
}


void camera(void *arg){
   int moniteur_status = 1;
   int etat,etat_default=ACTION_STOP_COMPUTE_POSITION;
   int ancien_etat;
   DMessage* message;
   DJpegimage *jpg;
   DImage *image;
   

 
	
   rt_printf("tcamera : Debut de l'éxecution de periodique à 600ms\n");
   
   rt_task_set_periodic(NULL, TM_NOW, 600000000);
   //Cette ligne sert juste à se régaler avec la caméra.
   //rt_task_set_periodic(NULL, TM_NOW, 40000000);

   camera_v->open(camera_v);
   if (camera_v != NULL){

   while(1){
     	rt_task_wait_period(NULL);
	    rt_printf("tcamera : Activation périodique\n");


	      rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        moniteur_status = etatCommMoniteur;
        rt_mutex_release(&mutexEtat);
	
	   if(moniteur_status==STATUS_OK){
		    rt_mutex_acquire(&mutexCamera, TM_INFINITE);
        etat=etatCamera;
        rt_mutex_release(&mutexCamera);

		switch(etat){
			case ACTION_FIND_ARENA:
			rt_printf("Détection d'arène ambrosini");
			rt_mutex_acquire(&mutexArene, TM_INFINITE);
			arene=d_new_arena();
			rt_mutex_release(&mutexArene);
			jpg=take_picture(1);
			message=d_new_message();
			message->put_jpeg_image(message,jpg);
			rt_mutex_acquire(&mutexCom, TM_INFINITE);
             		
			if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                		message->free(message);
                	}
                	rt_mutex_release(&mutexCom);
			break;


			case ACTION_STOP_COMPUTE_POSITION:
			jpg=take_picture(0);
			message=d_new_message();
			message->put_jpeg_image(message,jpg);
			rt_mutex_acquire(&mutexCom, TM_INFINITE);
             		if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                		message->free(message);
                	}
                	rt_mutex_release(&mutexCom);
			break;

			case ACTION_ARENA_IS_FOUND:
			rt_mutex_acquire(&mutexCamera, TM_INFINITE);
              		etatCamera=etat_default;
                  	rt_mutex_release(&mutexCamera);
			break;

			case ACTION_ARENA_FAILED:
			rt_mutex_acquire(&mutexCamera, TM_INFINITE);
              		etatCamera=etat_default;
                  	rt_mutex_release(&mutexCamera);
			arene->free(arene);
			break;

			case ACTION_COMPUTE_CONTINUOUSLY_POSITION:
			rt_printf("Détection de la position du robot mon pote");
			image = d_new_image();
			camera_v->get_frame(camera_v, image);
			rt_mutex_acquire(&mutexArene, TM_INFINITE);
			if (arene!=NULL){
				rt_mutex_acquire(&mutexPosition, TM_INFINITE);
				position=image->compute_robot_position(image,arene);
				rt_mutex_release(&mutexPosition);
			}
			rt_mutex_release(&mutexArene);
			if (position!=NULL){
				rt_mutex_acquire(&mutexPosition, TM_INFINITE);
				d_imageshop_draw_position(image,position);
				
				message=d_new_message();
				message->put_position(message,position);
				rt_mutex_release(&mutexPosition);
				rt_mutex_acquire(&mutexCom, TM_INFINITE);
             			if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                			message->free(message);
                		}
                		rt_mutex_release(&mutexCom);
				message=d_new_message();
				if (image != NULL){
					jpg->compress(jpg,image);
      					image->free(image);
				}
				
				message->put_jpeg_image(message,jpg);

				rt_mutex_acquire(&mutexCom, TM_INFINITE);
             			if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                			message->free(message);
                		}
                		rt_mutex_release(&mutexCom);
				
			}
			
			}

	}   
     
   }

}

}





void deplacer(void *arg) {

    rt_sem_p(&semConnectedRobot, TM_INFINITE);

    int status = 1;
    int gauche;
    int droite;
    int robot_status;
    DMessage *message;

    rt_printf("tmove : Debut de l'éxecution de periodique à 1s\n");
    rt_task_set_periodic(NULL, TM_NOW, 1000000000);

    while (1) {
        /* Attente de l'activation périodique */
        rt_task_wait_period(NULL);
        rt_printf("tmove : Activation périodique\n");

        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        robot_status = etatCommMoniteur;
        rt_mutex_release(&mutexEtat);

        if (robot_status == STATUS_OK){

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

          
        }
        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        etatCommRobot = status;
        rt_mutex_release(&mutexEtat);
        checkConnection(robot_status);
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
