float Q_rsqrt( float number )
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    i  = * ( long * ) &y;                       
    i  = 0x5f3759df - ( i >> 1 );                
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );
    return 1/y;
}

__kernel void TestKernel(
	__global float* points,
	__global float* masses,
	__global float* accel,
	__global float* speed,
	__global float* g_vertex_buffer_data,
	int elementsNumber,
	const float dt,
	const float G,
	unsigned int metresConstraint)
{
	//Get index into global data array
    int iJob = get_global_id(0);
	
	//Check boundary conditions
    if (iJob >= elementsNumber) return; 
	
	//Perform calculations
	float temp[3] = { 0, 0, 0 };
	for(int i = 0; i < elementsNumber; i++) {
		float distance = 0;
		if(i == iJob)
			continue;
			
		distance += (points[3 * iJob + 0] - points[3 * i + 0])*(points[3 * iJob + 0] - points[3 * i + 0]);
		distance += (points[3 * iJob + 1] - points[3 * i + 1])*(points[3 * iJob + 1] - points[3 * i + 1]);
		distance += (points[3 * iJob + 2] - points[3 * i + 2])*(points[3 * iJob + 2] - points[3 * i + 2]);
		distance = Q_rsqrt(distance * distance * distance);
		
		temp[0] = masses[i]*(points[3 * iJob + 0] - points[3 * i + 0]) / distance;
		temp[1] = masses[i]*(points[3 * iJob + 1] - points[3 * i + 1]) / distance;
		temp[2] = masses[i]*(points[3 * iJob + 2] - points[3 * i + 2]) / distance;
	};
	
	temp[0] *= -G;
	temp[1] *= -G;
	temp[2] *= -G;
	
	float new_speed[3];
	new_speed[0] = (temp[0] + accel[3 * iJob + 0]) * dt / 2;     
	new_speed[1] = (temp[1] + accel[3 * iJob + 1]) * dt / 2;
	new_speed[2] = (temp[2] + accel[3 * iJob + 2]) * dt / 2;	
	
	points[3 * iJob + 0] = (new_speed[0] + speed[3 * iJob + 0]) * dt / 2;
	points[3 * iJob + 1] = (new_speed[1] + speed[3 * iJob + 1]) * dt / 2;
	points[3 * iJob + 2] = (new_speed[2] + speed[3 * iJob + 2]) * dt / 2;
	
	speed[3 * iJob + 0] += new_speed[0];
	speed[3 * iJob + 1] += new_speed[1];
	speed[3 * iJob + 2] += new_speed[2];
	
	accel[3 * iJob + 0] += temp[0];
	accel[3 * iJob + 1] += temp[1];
	accel[3 * iJob + 2] += temp[2];
	
	g_vertex_buffer_data[3 * iJob + 0] = points[3 * iJob + 0] / (float)metresConstraint;
	g_vertex_buffer_data[3 * iJob + 1] = points[3 * iJob + 1] / (float)metresConstraint;
	g_vertex_buffer_data[3 * iJob + 2] = points[3 * iJob + 2] / (float)metresConstraint;
}