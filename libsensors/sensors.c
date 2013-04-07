#define LOG_TAG "Sensors"
#define LOG_NDEBUG 0

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <linux/input.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>

#include <sys/time.h>
#include <time.h>

#include "sensors.h"
/*****************************************************************************/

#define ID_ACC  			(0)
#define ID_MAGE  			(1)
#define ID_ORIEN  			(2)
#define ID_GYROS   			(3)
#define ID_LIGHT   			(4)
#define ID_PRESS   			(5)
#define ID_TEMP   			(6)
#define ID_PROX  			(7)
#define SENSORS_COUNT 			(ID_PROX+1)

#define SENSORS_ACCELEROMETER   	(1<<ID_ACC)
#define SENSORS_MAGNETIC_FIELD 		(1<<ID_MAGE)
#define SENSORS_ORIENTATION    		(1<<ID_ORIEN)
#define SENSORS_PROXIMITY		(1<<ID_PROX)
#define SENSORS_LIGHT	        	(1<<ID_LIGHT)
#define SENSORS_PRESSURE		(1<<ID_PRESS)
#define SENSORS_GYROSCOPE		(1<<ID_GYROS)
#define SENSORS_TEMPERATURE		(1<<ID_TEMP)

/*****************************************************************************/
// sensor IDs must be a power of two and
// must match values in SensorManager.java
#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Y
#define EVENT_TYPE_ACCEL_Z          ABS_Z
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_RUDDER

#define EVENT_TYPE_MAGV_X           ABS_HAT0X
#define EVENT_TYPE_MAGV_Y           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE

#define EVENT_TYPE_PRESS            ABS_PRESSURE 

#define EVENT_TYPE_TEMPERATURE      ABS_THROTTLE
#define EVENT_TYPE_STEP_COUNT       ABS_GAS

#define EVENT_TYPE_PROXIMITY        ABS_DISTANCE
#define EVENT_TYPE_LIGHT            ABS_MISC
// 720 LSG = 1G
//#define LSG      			(64.0f)
static float lsg = 64.0f;
// conversion of acceleration data to SI units (m/s^2)
#define CONVERT_A                   (GRAVITY_EARTH / lsg)
#define CONVERT_A_X                 (CONVERT_A)
#define CONVERT_A_Y                 (CONVERT_A)
#define CONVERT_A_Z                 (CONVERT_A)

#define CONVERT_M                   (1.0f*0.06f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

#define CONVERT_O                   (1.0f/lsg)
#define CONVERT_O_A                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (CONVERT_O)

#define SENSOR_STATE_MASK           (0x7FFF)

/*****************************************************************************/

struct sensors_context_t {
	struct sensors_poll_device_t device;
	struct sensors_event_t sensor_events[SENSORS_COUNT];
	int con_fds[SENSORS_COUNT];
	int data_fds[SENSORS_COUNT];
	int events;
};

static const char *sensor_device_names[SENSORS_COUNT + 1] = {
	G_SENSOR_NAME,
	MAGNETIC_SENSOR_NAME,
	ORIENTATION_SENSOR_NAME,
	GYROSCOPE_SENSOR_NAME,
	LIGHT_SENSOR_NAME,
	PRESSURE_SENSOR_NAME,
	TEMPERATURE_SENSOR__NAME,
	PROXIMITY_SENSOR_NAME,
};

static struct sensor_t sensors_list[SENSORS_COUNT];
/*****************************************************************************/

static int sensor_devices_activate(struct sensors_context_t *dev,int handle, int enabled)
{
	if (ioctl(dev->con_fds[handle],SENSOR_IOCTL_SET_ACTIVE, &enabled) < 0)
		ALOGE("handle:%d ECS_IOCTL_SET_ACTIVE error (%s)",handle, strerror(errno));
	return 0;
}


static int sensor_devices_setDelay(struct sensors_context_t *dev,int handle, int64_t ns)
{
	unsigned int ms = ns / 1000000;
	if (ioctl(dev->con_fds[handle],SENSOR_IOCTL_SET_DELAY, &ms) < 0)
		ALOGE ("handle:%d ECS_IOCTL_SET_DELAY error (%s)",handle, strerror(errno));
	return 0;
}


static int report_data(struct sensors_context_t *dev,sensors_event_t* data,int count)
{
	int i;
	struct timespec curtime;    
	curtime.tv_sec = curtime.tv_nsec = 0;

	for(i=0;i<SENSORS_COUNT;i++) {
		if (dev->events & (1<<i)) {
			dev->events &= ~(1<<i);
			*data = dev->sensor_events[i];
			clock_gettime(CLOCK_MONOTONIC, &curtime);
			data->timestamp = curtime.tv_sec*1000000000LL + curtime.tv_nsec;
			return 1;
		}
	}

	usleep(1000);
	return -1;
}

#define MAX(a,b)  (((a) > (b)) ? (a) : (b))
static int sensor_devices_poll(struct sensors_context_t *dev,sensors_event_t* data, int count)
{
	uint32_t v;
	fd_set rfds;
	int i, n, rfd = -1, nfds = 0;
	struct input_event event;

	if (dev->events)
		return report_data(dev,data,count);

	FD_ZERO(&rfds);
	for (i = 0; i < SENSORS_COUNT; i++) {
		if (dev->data_fds[i] < 0)
			continue;
		FD_SET(dev->data_fds[i], &rfds);
		nfds = MAX(nfds, dev->data_fds[i]);
	}
	nfds += 1;

	while (1) {
		n = select(nfds, &rfds, NULL, NULL, NULL);
		if (n < 0 && errno == EINTR)
			continue;

		for(i = 0; i < SENSORS_COUNT; i++) {
			if (dev->data_fds[i] < 0)
				continue;
			if (FD_ISSET(dev->data_fds[i], &rfds)) {
				rfd = dev->data_fds[i];
				break;
			}
		}

		if (read(rfd, &event, sizeof(event)) != sizeof(event)) 
			return -EINVAL;
		if (event.type == EV_ABS) {
			switch (event.code) {
				case EVENT_TYPE_ACCEL_X:
					dev->events |= SENSORS_ACCELEROMETER;
					dev->sensor_events[ID_ACC].acceleration.x = event.value * CONVERT_A_X;
					break;
				case EVENT_TYPE_ACCEL_Y:
					dev->events |= SENSORS_ACCELEROMETER;
					dev->sensor_events[ID_ACC].acceleration.y = -event.value * CONVERT_A_Y;
					break;
				case EVENT_TYPE_ACCEL_Z:
					dev->events |= SENSORS_ACCELEROMETER;
					dev->sensor_events[ID_ACC].acceleration.z = event.value * CONVERT_A_Z;
					break;

				case EVENT_TYPE_MAGV_X:
					dev->events |= SENSORS_MAGNETIC_FIELD;
					dev->sensor_events[ID_MAGE].magnetic.x = event.value * CONVERT_M_X;
					break;
				case EVENT_TYPE_MAGV_Y:
					dev->events |= SENSORS_MAGNETIC_FIELD;
					dev->sensor_events[ID_MAGE].magnetic.y = event.value * CONVERT_M_Y;
					break;
				case EVENT_TYPE_MAGV_Z:
					dev->events |= SENSORS_MAGNETIC_FIELD;
					dev->sensor_events[ID_MAGE].magnetic.z = event.value * CONVERT_M_Z;
					break;

				case EVENT_TYPE_YAW:
					dev->events |= SENSORS_ORIENTATION;
					dev->sensor_events[ID_ORIEN].orientation.azimuth =  event.value * CONVERT_O_A;
					break;
				case EVENT_TYPE_PITCH:
					dev->events |= SENSORS_ORIENTATION;
					dev->sensor_events[ID_ORIEN].orientation.pitch = event.value * CONVERT_O_P;
					break;
				case EVENT_TYPE_ROLL:
					dev->events |= SENSORS_ORIENTATION;
					dev->sensor_events[ID_ORIEN].orientation.roll = -event.value * CONVERT_O_R;
					break;

				case EVENT_TYPE_ORIENT_STATUS:
					v = (uint32_t)(event.value & SENSOR_STATE_MASK);
					//ALOGD_IF(dev->sensor_events[ID_ORIEN].orientation.status != (uint8_t)v,
					//		"M-Sensor status %d", v);
					dev->sensor_events[ID_ORIEN].orientation.status = (uint8_t)v;
					break;

				case EVENT_TYPE_LIGHT:
					dev->events |= SENSORS_LIGHT;
					dev->sensor_events[ID_LIGHT].light = event.value;
					break;
				case EVENT_TYPE_TEMPERATURE:
					dev->events |= SENSORS_TEMPERATURE;
					dev->sensor_events[ID_TEMP].temperature = event.value;
					break;
				case EVENT_TYPE_PROXIMITY:
					dev->events |= SENSORS_PROXIMITY;
					ALOGD("distance=%d", event.value);
					dev->sensor_events[ID_PROX].distance = event.value;
					break;
				case EVENT_TYPE_PRESS:
					//add code here for the press data
					break;
			}
		} else if (event.type == EV_SYN) {
			if (dev->events)
				return report_data(dev,data,count);
		}
	}
}

/*****************************************************************************/

static int open_con_fds(struct sensors_context_t* dev)
{
	int i,j,fd,count;
	char name[256];

	for(i = 0,count = 0;i < SENSORS_COUNT;i++) {
		memset(name,0,256);
		sprintf(name,"/dev/%s",sensor_device_names[i]);
		fd = open(name,O_RDWR);
		if(fd > 0) {
			dev->con_fds[i] = fd;
			count++;

			dev->sensor_events[i].version = sizeof(struct sensors_event_t);
			dev->sensor_events[i].sensor = i;
			for(j=0;j<SENSORS_COUNT;j++) {
				if(i == sensors_list[j].handle) {
					dev->sensor_events[i].type = sensors_list[j].type;
					break;
				}
			}
			ALOGD("find device:%s,fd:%d,sensor=%d.",name,fd,i);
		}
	}

	return count;
}

static int open_data_fds(struct sensors_context_t* dev)
{
	DIR *dir;
	struct dirent *de;
	int i,fd,count;
	char filename[PATH_MAX] = {0,};
	char inputname[256];

	dir = opendir("/dev/input/");
	if(dir == NULL) return -1;

	count = 0;
	while((de = readdir(dir))) {
		if(!strcmp(de->d_name,"..") || !strcmp(de->d_name,"."))
			continue;

		sprintf(filename,"/dev/input/%s",de->d_name);
		fd = open(filename, O_RDWR);

		if (fd < 0) {
			continue;
		}

		if (ioctl(fd, EVIOCGNAME(sizeof(inputname) - 1), &inputname) < 1) {
			close(fd);
			continue;
		}

		for(i=0;i<SENSORS_COUNT;i++){
			if (!strcmp(inputname,sensor_device_names[i])){
				ALOGD("using %s (name=%s) fd:%d", sensor_device_names[i], inputname, fd);
				dev->data_fds[i] = fd;
				count++;
				break;
			}
		}

		if(i == SENSORS_COUNT)
			close(fd);
	}
	closedir(dir);
	return count;
}

static int sensor_devices_common_close(struct hw_device_t* device)
{
	int i;
	if(device) {
		struct sensors_context_t *dev = (struct sensors_context_t *)device;
		for(i=0;i<SENSORS_COUNT;i++) {
			if(dev->con_fds[i] > 0) 
				close(dev->con_fds[i]);
			if(dev->data_fds[i] > 0) 
				close(dev->data_fds[i]);
		}
		free(dev);
	}

	return 0;
}

static int sensor_devices_common_open(struct sensors_context_t* dev)
{
	int i,ret;
	ret = open_con_fds(dev);
	if(ret < 1) {
		sensor_devices_common_close((hw_device_t*)dev);
		return -1;
	}

	ret = open_data_fds(dev);
	if(ret < 1) {
		sensor_devices_common_close((hw_device_t*)dev);
		return -1;
	}

	return 0;
}

/*****************************************************************************/

static uint32_t sensors_get_list(struct sensors_module_t* module,struct sensor_t const** list) 
{
	int i,fd;
	char name[256];
	static int count = 0;
	struct linux_sensor_t sensor_tmp;

	if(count != 0) {
		*list = sensors_list;
		return count;
	}

	for(i = 0; i < SENSORS_COUNT;i++)
	{
		memset(name,0,256);
		sprintf(name,"/dev/%s",sensor_device_names[i]);
		fd = open(name,O_RDWR);
		if(fd < 0) {
			continue;
		}

		if (ioctl(fd,SENSOR_IOCTL_GET_DATA, &sensor_tmp) < 0) {
			close(fd);
			continue;
		}

		close(fd);
		switch(sensor_tmp.type)
		{
			case SENSOR_TYPE_ACCELEROMETER:  
			        lsg = sensor_tmp.maxRange;	
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_ACC; 
				break;               
			case SENSOR_TYPE_MAGNETIC_FIELD:     
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_MAGE; 
				break;               
			case SENSOR_TYPE_ORIENTATION:         
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_ORIEN;
				break;               
			case SENSOR_TYPE_GYROSCOPE:           
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_GYROS;
				break;               
			case SENSOR_TYPE_LIGHT:
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_LIGHT;
				break;               
			case SENSOR_TYPE_PRESSURE:
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_PRESS; 
				break;            
			case SENSOR_TYPE_TEMPERATURE:    
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_TEMP; 
				break;     
			case SENSOR_TYPE_PROXIMITY:
				sensors_list[count].handle  =  SENSORS_HANDLE_BASE+ID_PROX;
				break;
			default:
				continue;
		}

		sensors_list[count].name = strdup(sensor_tmp.name);
		sensors_list[count].vendor = strdup(sensor_tmp.vendor); 
		sensors_list[count].type = sensor_tmp.type;
		sensors_list[count].version = sensor_tmp.version;
		sensors_list[count].maxRange = sensor_tmp.maxRange;
		sensors_list[count].resolution = 1.0 / (float)sensor_tmp.resolution;
		sensors_list[count].power = sensor_tmp.power;
		ALOGD("%d %s\n",count,sensors_list[count].name);

		count ++;
	}

	*list = sensors_list;
	return count; 
}

/********************************************************************************************************************/

static int open_sensors(const struct hw_module_t* module, const char* name,
		struct hw_device_t** device)
{
	int i;
	if (!strcmp(name, SENSORS_HARDWARE_POLL)) {
		struct sensors_context_t *dev = malloc(sizeof(*dev));

		memset(dev, 0, sizeof(*dev));
		for(i=0;i<SENSORS_COUNT;i++) {
			dev->con_fds[i] = -1;
			dev->data_fds[i] = -1;
		}

		dev->device.activate 	= sensor_devices_activate;
		dev->device.setDelay 	= sensor_devices_setDelay;
		dev->device.poll 	= sensor_devices_poll;

		dev->device.common.tag = HARDWARE_DEVICE_TAG;
		dev->device.common.version = 1;
		dev->device.common.module = module;
		dev->device.common.close = sensor_devices_common_close;
		*device = &dev->device.common;

		return sensor_devices_common_open(dev);
	}
	return -EINVAL;
}

static struct hw_module_methods_t sensors_module_methods = {
	.open = open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.version_major = 1,
		.version_minor = 0,
		.id = SENSORS_HARDWARE_MODULE_ID,
		.name = "Ingenic sensors Module",
		.author = "ztyan@ingenic.cn",
		.methods = &sensors_module_methods,
	},
	.get_sensors_list = sensors_get_list,
};


