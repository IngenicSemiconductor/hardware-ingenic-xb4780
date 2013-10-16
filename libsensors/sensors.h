/* include/linux/sensors.h
 *
 *  Copyright (C) 2011 Ingenic Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _LINUX_SENSORS_H
#define _LINUX_SENSORS_H

#define G_SENSOR_NAME "g_sensor"
#define MAGNETIC_SENSOR_NAME "magnetic_sensor"
#define ORIENTATION_SENSOR_NAME "orientation_sensor"
#define GYROSCOPE_SENSOR_NAME "gyroscope_sensor"
#define LIGHT_SENSOR_NAME "light_sensor"
#define PRESSURE_SENSOR_NAME "pressure_sensor"
#define TEMPERATURE_SENSOR__NAME "temperature_sensor"
#define PROXIMITY_SENSOR_NAME "proximity_sensor"


struct  sensors_emenation_t {
	/*use for the G-sensor*/
      short *matrix;
    	int8_t status;
    	uint8_t reserved[3];
};
struct linux_sensor_t {
    	/* name of this sensors */
   		char     name[100];
    	/* vendor of the hardware part */
    	char     vendor[100];

			int type;
    	/* version of the hardware part + driver. The value of this field is
     	* left to the implementation and doesn't have to be monotonicaly
     	* increasing.
     	*/    
    	int             version;
    	/* maximaum range of this sensor's value in SI units */
    	int           maxRange;
    	/* smallest difference between two values reported by this sensor */
    	int           resolution;
    	/* rough estimate of this sensor's power consumption in mA */
    	int           power;
    	/* reserved fields, must be zero */
    	void*           reserved[9];
};

struct sensors_platform_data {
			int intr;
			struct sensors_emenation_t *plate_data;
			void *plate; // 个别设备需要传递的参数.
};

// Sensor types are defined in hardware/libhardware/include/hardware/sensors.h

/*
	define it for all sensor interface .
	you can add a interface here.
*/
#define	SENSORIO	0x4c
/*you must realize  the list below interface*/
#define	SENSOR_IOCTL_SET_DELAY		_IOW(SENSORIO, 0x11, short)
#define	SENSOR_IOCTL_SET_ACTIVE		_IOW(SENSORIO, 0x13, short)
#define	SENSOR_IOCTL_GET_DATA		_IOR(SENSORIO, 0x15, short)

/*you can chose it for realize it for your driver*/
#define	SENSOR_IOCTL_GET_DELAY		_IOR(SENSORIO, 0x12, short)
#define	SENSOR_IOCTL_GET_ACTIVE		_IOR(SENSORIO, 0x14, short)

#define	SENSOR_IOCTL_GET_DATA_TYPE		_IOR(SENSORIO, 0x16, short)
#define	SENSOR_IOCTL_GET_DATA_MAXRANGE		_IOR(SENSORIO, 0x17, short)
#define	SENSOR_IOCTL_GET_DATA_NAME		_IOR(SENSORIO, 0x18, short)
#define	SENSOR_IOCTL_GET_DATA_POWER		_IOR(SENSORIO, 0x19, short)
#define	SENSOR_IOCTL_GET_DATA_RESOLUTION	_IOR(SENSORIO, 0x20, short)
#define	SENSOR_IOCTL_GET_DATA_VERSION		_IOR(SENSORIO, 0x21, short)


#endif /* _LINUX_LIS35DE_H */

