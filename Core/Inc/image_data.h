/*
 * image_data.h
 *
 *  Created on: 5 мар. 2021 г.
 *      Author: alexm
 */

#ifndef INC_IMAGE_DATA_H_
#define INC_IMAGE_DATA_H_

#define IMAGE_TITLE_SIZE	128
#define IMAGE_URL_SIZE		256

struct _image_data_t {
	size_t size;
	char title[IMAGE_TITLE_SIZE];
	char url[IMAGE_URL_SIZE];
};
typedef struct _image_data_t image_data_t;

#endif /* INC_IMAGE_DATA_H_ */
