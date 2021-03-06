#include "Energia.h"
#include "Engine.h"

Engine::Engine(){
  J	= 0.2;			// Moment of Inertia
  k1	= 2.5;			// Combustion process/efficiency
  kf0 = 0.2;			// static coefficient of friction
  kf1 = 0.002;		// coefficient of friction
  kf2 = 0.0006;		// Squared coefficient of friction
  
  
  // Working variables
  w	= 2;			// Angular speed in rads/s
  throttle = 0;                 // Throttle position: 0-100 %
  F	= 0;			// Fuel pulse width
  T	= 0;			// Torque (Nm)
  TN	= 0;			// Net Torque (Nm)
  TF	= 0;			// Friction Torque (Nm)
  TL	= 0;			// Load Torque (Nm)
  s	= 0;			// Engine speed in RPM
  wdot  = 0;			// Angular acceleration (rads/s-2)
  AFR   = 14.7;                 // Calculated AFR
  lambda = 1;                    // Calculated Equivalence ratio
  ignition = 0;
}

int Engine::test(int in)
{
  return in*2;  
}

void Engine::reset(){
 w = 0; 
}

void Engine::crank(int w){
 this->w = w; 
}

void Engine::simulate(float dt){
  
  // Limit the fuelling between 0 and 100%
  if(this->throttle > 100){
   this->throttle = 100; 
  }
  if(this->throttle < 0){
    this->throttle = 0;
  }
  
  if(this->s > 0 && this->throttle > 0 && this->F > 0){ // Checks for floating point calcs
    this->AFR = (23814 + 0.004 * this->s * this->throttle)/(this->F);
    this->lambda = (this->AFR) / 14.7;
  }else{
    this->AFR = 0.0;
    this->lambda = 0.0; 
  }
  
  this->T = this->k1 * this->throttle;						// Torque from Combustion

  float scale = -1.1536*pow(this->lambda,2) + 2.6954*this->lambda - 0.5695;     // Scale torque for equivalence ratio (inferred from diagral 4.1 in Stone)

  if( scale < 0.7 ){      // Limit the scaling
     scale = 0.7; 
  }
  this->T = this->T * scale;
  
  this->TF = this->kf0*(this->w>0?1:0) + this->kf1*this->w + this->kf2 * pow(this->w, 2);	// Friction Torque
  
  this->TF = this->w < 0? -this->TF:this->TF;// If the engine is turning backwards, then the friction works in the opposite direction
  
  this->TN = this->T - this->TF;				// Net torque = combustion torque - friction
  this->TN = this->TN - this->TL;				// Net torque minus the load
  
  this->wdot = this->TN / this->J;				// Angular acceleration = Torque/inertia
  this->w = this->w + this->wdot * dt;				// Engine speed = acceleration * timebase
  this->s = this->w * 60 / (2 * PI);                            // Convert engine speed from rads/s to RPM
  
  
}
