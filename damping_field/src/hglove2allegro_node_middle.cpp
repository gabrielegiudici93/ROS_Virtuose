#include "ros/ros.h"
#include "std_msgs/String.h"
#include "std_msgs/Float64.h"
#include "virtuose/out_virtuose_status.h"
#include "virtuose/out_virtuose_pose.h"
#include "virtuose/out_virtuose_physical_pose.h"

#include "virtuose/in_virtuose_force.h"
#include "virtuose/virtuose_impedance.h"
#include "virtuose/virtuose_reset.h"
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/PoseArray.h"
#include <geometry_msgs/WrenchStamped.h>
#include <iostream>
#include <fstream>
#include <cmath> // Include the cmath library for sqrt function


////////////////////////////////////////////////THIS CODE REQUIRES $rosrun virtuose thumb_node and $rosrun damping_field hglove2allegro_thumb RUNNING IN A SEPARATE SHELL
// Storage for virtuose pose and ur5 position
float cur_pose_H[7];//pose ->virtuose
float curr_wrench[4];
float force_norm;
float old_pose[7];
float initial_pose_H[7];
float delta_pos_H[7];
float delta_vel_H[7];
uint64_t status_date;
float x_h[3];
float x_AH[7];
float damp=1;
//SET THE FORCE FEEDBACK MODE 0 FALSE 1 TRUE
int activate_spring_test=0;
int activate_optoforce_wrench=0; //you need to uncomment line 153  // ros::Subscriber optoForce0_feedback = n_in.subscribe("/optoforce_wrench_1", 5, __OnOPTO0Contact);
int activate_Crisp_sensor_wrench=1; //you need to uncomment line 155 // ros::Subscriber optoForce0_feedback = n_in.subscribe("/Crisp_IN_2HGlove", 5, __OnCrispFingertipIN);


int old_H_flag=0;
uint64_t pose_date;
int dt=0.01;
//
//Sping Only parameters: force limit=10; KK2=50
float error_x=0, error_y =0, error_z= 0;
float error_sensitivity=0.001;//0.01;
float opto_sensitivity=0.50;
float crisp_sensitivity=30;
float max_treshold=10000;

float force_limit=10;
float damping_coeff=0;//100;
float damping_force[3];
float fixed_spring_x,fixed_spring_y,fixed_spring_z;
float spring_force[3];
float fixed_spring_x_force,fixed_spring_y_force,fixed_spring_z_force;
float curr_spring_x_force,curr_spring_y_force, curr_spring_z_force;
float test_error;
bool new_contact=0;
float initial_contact_pose[7];
float contact_pose[7];
float delta_contact_diplacement[7];
//
float virutal_elong__H[3];
float KK=0;
float KK_fix=0;//100;// for VIRUTOSE ALONE SPRING TEST set 100;
float KK_curr=0;//1000;
// Storage for virtuose_node status
int status_state = 0;
int status_button;
float test_x,test_y,test_z;  

ros::Publisher* _out_virtuose_delta_pose;
 

// Storage for virtuose pose
float cur_pose[7], init_pose[7], delta_pose[7];
bool init_pose_record=false;

// ??tf2::Quaternion delta_quat, init_quat, cur_quat;


// Define a function to compute the norm of a 3D vector
float computeNorm(const float* vector) {
    // Calculate the squared sum of the components
    double sum_of_squares = vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2];
    
    // Return the square root of the sum
    return sqrt(sum_of_squares);
}


// Callback for topic out_virtuose_pose
void out_virtuose_poseCB(const virtuose::out_virtuose_pose::ConstPtr& msg)
{
// Store last pose date
  pose_date = msg->header.stamp.toNSec();
  cur_pose_H[0] = msg->virtuose_pose.translation.x; //virtuose_pose  cambia solo se si impugna con decisione il manettino
  cur_pose_H[1] = msg->virtuose_pose.translation.y;
  cur_pose_H[2] = msg->virtuose_pose.translation.z;
  cur_pose_H[3] = msg->virtuose_pose.rotation.x;
  cur_pose_H[4] = msg->virtuose_pose.rotation.y;
  cur_pose_H[5] = msg->virtuose_pose.rotation.z;
  cur_pose_H[6] = msg->virtuose_pose.rotation.w;

if(cur_pose_H[0]!=0.0 && cur_pose_H[1]!=0.0 && cur_pose_H[2]!=0.0)
  {
    if (old_H_flag==0) 
  {
    for(int i=0;i<7;i++)
    {
      initial_pose_H[i]=cur_pose_H[i];
      old_pose[i]=initial_pose_H[i];
    }
    old_H_flag=1;
  } 
  }
}

//SENSORS CALLBACKS 
void __OnOPTO0Contact(const geometry_msgs::WrenchStamped::ConstPtr& msg)
{
  //std::cout << " OPTOFORCE 0 "<< "x" << msg->wrench.force.x <<"y" << msg->wrench.force.y <<"z" << msg->wrench.force.z <<"\n";
  curr_wrench[0]= msg->wrench.force.x;
  curr_wrench[1]= msg->wrench.force.y;
  curr_wrench[2]= msg->wrench.force.z;
}

void __OnCrispFingertipMID(const geometry_msgs::WrenchStamped::ConstPtr& msg)
{
  //std::cout << " OPTOFORCE 0 "<< "x" << msg->wrench.force.x <<"y" << msg->wrench.force.y <<"z" << msg->wrench.force.z <<"\n";
  curr_wrench[0]= msg->wrench.force.x;
  curr_wrench[1]= msg->wrench.force.y;
  curr_wrench[2]= msg->wrench.force.z;
  curr_wrench[3] = computeNorm(curr_wrench);

 if (curr_wrench[2]<= crisp_sensitivity)
   new_contact=0;
 else 
   new_contact=1;
}


// Callback for topic out_virtuose_status
void out_virtuose_statusCB(const virtuose::out_virtuose_status::ConstPtr& msg)
{
  // Store last status date
  status_date = msg->header.stamp.toNSec();
  status_state = msg->state;
  status_button = msg->buttons;
}

void __OnFingerPos(const geometry_msgs::PoseArray::ConstPtr& msg)
{
  //std::cout << " OPTOFORCE 0 "<< "x" << msg->wrench.force.x <<"y" << msg->wrench.force.y  // ros::Subscriber optoForce0_feedback = n_in.subscribe("/optoforce_wrench_1", 5, __OnOPTO0Contact);<<"z" << msg->wrench.force.z <<"\n";
  x_AH[0]= msg->poses[2].position.x;
  x_AH[1]= msg->poses[2].position.y;
  x_AH[2]= msg->poses[2].position.z;

  x_AH[3]= msg->poses[2].orientation.x;
  x_AH[4]= msg->poses[2].orientation.y;
  x_AH[5]= msg->poses[2].orientation.z;
  x_AH[6]= msg->poses[2].orientation.x;

if (new_contact==0)
   for(int i=0;i<7;i++)
   {
   delta_contact_diplacement[i]=0.0;  
   initial_contact_pose[i]=x_AH[i];
   }
else
   for(int i=0;i<7;i++)
   {
   contact_pose[i]=x_AH[i];
   delta_contact_diplacement[i]=contact_pose[i]-initial_contact_pose[i];
   }

}

// Main function
int main(int argc, char **argv)
{
  printf("Starting test_middle_impedance node\n");
  // Init ROS library
  ros::init(argc, argv, "middle_impedance");
  // Create node handle
  ros::NodeHandle n_mid;

  // Connect to virtuose_node
  printf("Connecting to virtuose node\n");
  ros::Subscriber out_virtuose_status = n_mid.subscribe<virtuose::out_virtuose_status>("out_middle_status", 1, out_virtuose_statusCB);
  ros::Subscriber out_virtuose_pose = n_mid.subscribe<virtuose::out_virtuose_pose>("out_middle_pose", 1, out_virtuose_poseCB);
  ros::Publisher in_virtuose_force = n_mid.advertise<virtuose::in_virtuose_force>("in_middle_force", 1);
  ros::Publisher in_stiffness = n_mid.advertise<std_msgs::Float64>("mid_stiffness", 5);

  //ros::Publisher out_virtuose_delta_pose = n_midadvertise<virtuose::out_virtuose_pose>("out_virtuose_delta_pose", 1);
  //_out_virtuose_delta_pose = &out_virtuose_delta_pose;

  ros::ServiceClient virtuose_impedance = n_mid.serviceClient<virtuose::virtuose_impedance>("middle_impedance");
  //ros::ServiceClient virtuose_impedance = n_midserviceClient<virtuose::virtuose_impedance>("index_impedance");
  //ros::ServiceClient virtuose_impedance = n_midserviceClient<virtuose::virtuose_impedance>("middle_impedance");

  ros::ServiceClient virtuose_reset = n_mid.serviceClient<virtuose::virtuose_reset>("virtuose_reset");


  //OPTOFORCE/CrispFingertips SUBSCRIBERS

    // ros::Subscriber optoForce0_feedback = n_mid.subscribe("/optoforce_wrench_2", 5, __OnOPTO0Contact);

    ros::Subscriber optoForce0_feedback = n_mid.subscribe("/Crisp_RING_2HGlove", 5, __OnCrispFingertipMID);
 

  // Reset virtuose_node
  virtuose::virtuose_reset res;
  // virtuose_reset.call(res);
  
  // Create multithread spinner
  ros::AsyncSpinner spinner(2);
  spinner.start();

  uint64_t start = ros::Time::now().toNSec();

  // Request impedance mode
  printf("Sending impedance request\n");
  virtuose::virtuose_impedance imp;
  imp.request.ip_address = "localhost#53213"; //11 middle SvcHaptic_HGlove_finger_middle_n5.conf
  imp.request.indexing_mode = 0;
  imp.request.speed_factor = 1.0;
  imp.request.force_factor = 1.0;
  imp.request.power_enable = 1;
  imp.request.max_force = 10.0;
  imp.request.max_torque = 1.0;
  imp.request.base_frame.translation.x = 0.0;
  imp.request.base_frame.translation.y = 0.0;
  imp.request.base_frame.translation.z = 0.0;
  imp.request.base_frame.rotation.x = 0.0;
  imp.request.base_frame.rotation.y = 0.0;
  imp.request.base_frame.rotation.z = 0.0;
  imp.request.base_frame.rotation.w = 1.0;
  if (!virtuose_impedance.call(imp))
  {
    printf("Call failed\n");
    return 0;
  }
  // Store client ID given by virtuose_node
  uint32_t client_id = 0;
  client_id = imp.response.client_id;
  printf("Our client ID is %d\n", client_id);

/*
IMPEDANCE CONTROLLER

*/


  // Perform impedance loop at 500 Hz
  ros::Rate r(500);
  int ctr = 0;
  while(ros::ok())
  {
// Create null force
    virtuose::in_virtuose_force force;
    force.header.stamp = ros::Time::now();
    force.client_id = client_id;
    force.virtuose_force.force.x = 0.0;
    force.virtuose_force.force.y = 0.0;
    force.virtuose_force.force.z = 0.0;
    force.virtuose_force.torque.x = 0.0;
    force.virtuose_force.torque.y = 0.0;
    force.virtuose_force.torque.z = 0.0;

    
//DELTA TRANSLATION NO ROTATION
    for(int j=0;j<3;j++)
    {
      delta_pos_H[j]=cur_pose_H[j] - old_pose[j];
      delta_vel_H[j]=delta_pos_H[j]/dt;
      old_pose[j]=cur_pose_H[j];
    }
    

    // myfile << "cur_pose_H"<< "," <<cur_pose_H[0]<< "," << cur_pose_H[1]<< "," <<cur_pose_H[2]<< ","  ;
    // myfile << "old_pose_H"<< "," <<old_pose[0]<< "," << old_pose[1]<< "," <<old_pose[2]<< ","  ;
    // myfile << "delta_pos H"<< "," <<delta_pos_H[0]<< "," << delta_pos_H[1]<< "," <<delta_pos_H[2]<< ",";


//VIRTUAL ELONG FIXED FRAME VIRTUOSE
// printf("initial pose: x %f - y %f - z %f \n",initial_pose_H[0],initial_pose_H[1],initial_pose_H[2]);
// printf("cur_pose_H : x %f - y %f - z %f \n\n",cur_pose_H[0],cur_pose_H[1],cur_pose_H[2]);

virutal_elong__H[0]=-(initial_pose_H[0]-cur_pose_H[0]);
virutal_elong__H[1]=-(initial_pose_H[1]-cur_pose_H[1]);
virutal_elong__H[2]=-(initial_pose_H[2]-cur_pose_H[2]);


    force_limit=10;
    KK_fix=50;// 100 STRONG FORCE, 10 LIGHT FORCE 
    KK_curr=0;//150;

fixed_spring_x_force=KK_fix*(virutal_elong__H[0]);
fixed_spring_y_force=KK_fix*(virutal_elong__H[1]);
fixed_spring_z_force=KK_fix*(virutal_elong__H[2]);

if (activate_spring_test==1){

spring_force[0]=-(fixed_spring_x_force)*5;
spring_force[1]=-(fixed_spring_y_force)*5;
spring_force[2]=-(fixed_spring_z_force)*5;
//printf("spring_force: x %f - y %f - z %f \n",spring_force[0],spring_force[1],spring_force[2]);
}

/////////////////////////////// OPTOFORCE ////////////////////////////////////
////DO NOT MODIFY --- FOR CRISP SENSOR READ BELOW ---
else if (activate_optoforce_wrench==1 && activate_Crisp_sensor_wrench==0)
{
  spring_force[0]= ( -(abs(curr_wrench[0])))/5;
  spring_force[1]=0;//- ( -(curr_wrench[1]))/10;
  spring_force[2]=0;// ( -(curr_wrench[0]))/10;//5 is a test to increase the force to reach the limit
  //GENERATE X FORCE
if (abs(curr_wrench[0])>opto_sensitivity||abs(curr_wrench[1])>opto_sensitivity||abs(curr_wrench[2])>opto_sensitivity)
{
  //printf("A\n");
  force.virtuose_force.force.x = spring_force[0];
  if (force.virtuose_force.force.x > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.x = force_limit;
    spring_force[0]=force_limit;
    //printf("WARNING: FORCE TRESHOLD X REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[0], force_limit);
  }
  else if (force.virtuose_force.force.x < -force_limit)
  {
    force.virtuose_force.force.x = -force_limit;
    spring_force[0]=-force_limit;
    //printf("WARNING: FORCE TRESHOLD X REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[0], force_limit);
  }     

//GENERATE Y FORCE

  force.virtuose_force.force.y = spring_force[1];
  if (force.virtuose_force.force.y > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.y = force_limit;
    spring_force[1]=force_limit;
    //printf("WARNING: FORCE TRESHOLD Y REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[1], force_limit);
  }
  else if (force.virtuose_force.force.y < -force_limit)
  {
    force.virtuose_force.force.y = -force_limit;
    spring_force[1]=-force_limit;
    // printf("WARNING: FORCE TRESHOLD Y REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[1], force_limit);
  }     


  //GENERATE Z FORCE

  force.virtuose_force.force.z = spring_force[2];
  if (force.virtuose_force.force.z > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.z = force_limit;
    spring_force[2]=force_limit;
    // printf("WARNING: FORCE TRESHOLD Z REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[2], force_limit);
  }
  else if (force.virtuose_force.force.z < -force_limit)
  {
    force.virtuose_force.force.z = -force_limit;
    spring_force[2]=-force_limit;
    // printf("WARNING: FORCE TRESHOLD Z REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[2], force_limit);
  }     
}
    else 
    {
            force.virtuose_force.force.x=0.0;
            force.virtuose_force.force.y = 0.0;
            force.virtuose_force.force.z=0.0;
    }

}




/////////////////////////////// CRISP SENSORS ////////////////////////////////////




// else if (activate_optoforce_wrench==0 & activate_Crisp_sensor_wrench==1)
// {
//       //2000 is sensor max treshold
//       max_treshold=600;//was 400
  
//     if(curr_wrench[2]<crisp_sensitivity){
//       spring_force[0]=0.0;
//     }else if (curr_wrench[2]<=max_treshold){
//       spring_force[0]=-abs(curr_wrench[2])/max_treshold*force_limit;//*thumb_extra_force_factor;
//     }else if (curr_wrench[2]>max_treshold){
//       spring_force[0]=-1*force_limit;
//     }
//     // if(curr_wrench[2]<-crisp_sensitivity){
//     //   spring_force[0]=-abs(curr_wrench[2])/max_treshold*force_limit/2;
//     // }

else if (activate_optoforce_wrench==0 && activate_Crisp_sensor_wrench==1)
{
      //2000 is sensor max treshold
      max_treshold=700;//was400
    if(curr_wrench[2]<crisp_sensitivity){
      spring_force[0]=0.0;
    }else if (curr_wrench[2]<=max_treshold){
      spring_force[0]=-abs(curr_wrench[2])/max_treshold*force_limit;// - abs((cur_pose_H-x_AH))*damp;//*thumb_extra_force_factor;
      // printf("Norm of the force vector: %f\n", force_norm);}

    // }else if (curr_wrench[2]>100 && curr_wrench[2]<=300){
    //   spring_force[0]=-force_limit/5;
    // // }else if (curr_wrench[2]>200 && curr_wrench[2]<=300){
    // //   spring_force[0]=-force_limit/4;
    // }else if (curr_wrench[2]>300 && curr_wrench[2]<=500){
    //   spring_force[0]=-force_limit/2;
    // // }else if (curr_wrench[2]>400 && curr_wrench[2]<=500){
    // //   spring_force[0]=-force_limit/2;
    // }else if (curr_wrench[2]>500 && curr_wrench[2]<=max_treshold ){
    //   spring_force[0]=-force_limit/1.5;

    }else if (curr_wrench[2]>max_treshold){
      spring_force[0]=-1*force_limit;
    }

    // spring_force[0]= ( -abs((curr_wrench[0])))/50;/////////////////////////////////////////////////////////////////////////////////////////////////////
    spring_force[1]=spring_force[0];//
    
    spring_force[2]=0;// 
    // printf("FORCE  X  %f \n", curr_wrench[0]);


//GENERATE X FORCE
{
  //printf("A\n");
  force.virtuose_force.force.x = spring_force[0];
  if (force.virtuose_force.force.x > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.x = force_limit;
    spring_force[0]=force_limit;
    //printf("WARNING: FORCE TRESHOLD X REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[0], force_limit);
  }
  else if (force.virtuose_force.force.x < -force_limit)
  {
    force.virtuose_force.force.x = -force_limit;
    spring_force[0]=-force_limit;
    //printf("WARNING: FORCE TRESHOLD X REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[0], force_limit);
  }     

//GENERATE Y FORCE

  force.virtuose_force.force.y = spring_force[1];
  if (force.virtuose_force.force.y > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.y = force_limit;
    spring_force[1]=force_limit;fixed_spring_x_force;
    //printf("WARNING: FORCE TRESHOLD Y REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[1], force_limit);
  }
  else if (force.virtuose_force.force.y < -force_limit)
  {
    force.virtuose_force.force.y = -force_limit;
    spring_force[1]=-force_limit;
    //printf("WARNING: FORCE TRESHOLD Y REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[1], force_limit);
  }     


  //GENERATE Z FORCE

  force.virtuose_force.force.z = spring_force[2];
  if (force.virtuose_force.force.z > force_limit) //10 is a safety treshold
  {
    force.virtuose_force.force.z = force_limit;
    spring_force[2]=force_limit;
    //printf("WARNING: FORCE TRESHOLD Z REACHED %f, SETTING SAFETY VALUE %f \n", spring_force[2], force_limit);
  }
  else if (force.virtuose_force.force.z < -force_limit)
  {
    force.virtuose_force.force.z = -force_limit;
    spring_force[2]=-force_limit;
    //printf("WARNING: FORCE TRESHOLD Z REACHED %f, SETTING SAFETY VALUE -%f \n", spring_force[2], force_limit);
  }     
}
// else 
// {
//         force.virtuose_force.force.x=0.0;
//         force.virtuose_force.force.y = 0.0;
//         force.virtuose_force.force.z=0.0;
// }

}





/*

BELOW FORCE SETTINGS AND SAFETY CHECKS

*/

//PUBLISH FORCE
    test_x=force.virtuose_force.force.x;
    test_y=force.virtuose_force.force.y;
    test_z=force.virtuose_force.force.z;


    if (abs(test_x)>force_limit || abs(test_y)>force_limit || abs(test_z)>force_limit)
      {
        printf("DANGEROUS FORCE FEEDBACK  TURN OFF THE GREEN BUTTON ");
        force.virtuose_force.force.x=0.0;
        force.virtuose_force.force.y = 0.0;
        force.virtuose_force.force.z=0.0;
      }

    
    // std::cout << "FORCE PUB -- x : -- " << force.virtuose_force.force.x << " -- y : -- " << force.virtuose_force.force.y << "-- z : -- " << force.virtuose_force.force.z<< "\n \n";
    // std::cout << "MIDDLE";
    in_virtuose_force.publish(force);
    
    ctr ++;
    // printf status every second
    // if (ctr % 500 == 0)
    // {
    //   printf("");
    //   //printf("Status: %ld %d %d %f %f %f\n", status_date - start, status_state, status_button,
    //    //      cur_pose_H[0], cur_pose_H[1], cur_pose_H[2]);
    // }
    r.sleep();
  }

// Reset virtuose_node
  virtuose_reset.call(res);

  return 0;
}
