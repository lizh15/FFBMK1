/*
  specific functions for pid device,
  to replenish MXCube customed usbd_hid
*/
#include "usbd_pid.h"
#include "usbd_hid.h"
#include "stm32f1xx_hal_pcd.h"
#include "ffb_manager.h"
#include "ffb_stick.h"

extern USBD_HandleTypeDef hUsbDeviceFS;
const uint16_t HID_Out_Report_Maxlen = 0x20;
const uint16_t HID_Joystick_In_Report_len = 7, HID_In_Report_Maxlen = 0x20;
uint8_t HID_Out_Report[HID_Out_Report_Maxlen];
uint8_t HID_In_Report[HID_In_Report_Maxlen];
uint8_t HID_Joystick_In_Report[HID_Joystick_In_Report_len];
uint8_t reportID, reportType;
uint16_t reportLen;
enum _PIDStat {
  PID_IDLE = 0,
  PID_INVALID,
  PID_SET_FEATURE,
  PID_GET_FEATURE,
  PID_GET_INPUT,
  PID_SET_OUTPUT,
} reqStat;

uint8_t USBD_PID_ItfReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
uint8_t USBD_PID_ReqServ(void);
uint8_t USBD_PID_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t USBD_PID_EP0_RxReady (USBD_HandleTypeDef *pdev);

uint8_t USBD_PID_ItfReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  /*Class Request Handler:Get_Report && Set_Report*/
  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_10);
  reportType = HIBYTE(req->wValue);
  reportID = LOBYTE(req->wValue);
  reportLen = req->wLength;
  reqStat = PID_IDLE;
  switch(req->bRequest) {
    case HID_REQ_GET_REPORT:
      switch(reportType) {
        case 0x03: //Feature
          reqStat = PID_GET_FEATURE;
          break;
        case 0x01: //Input
          reqStat = PID_GET_INPUT;
          break;
        default:
          reqStat = PID_INVALID;
          break;
      }
      break;
    case HID_REQ_SET_REPORT:
      if(req->wLength == 0) break; //don not receive 0-length out
      switch(reportType) {
        case 0x03: //Feature EP0 Out
          reqStat = PID_SET_FEATURE;
          break;
        case 0x02: //Output
          reqStat = PID_SET_OUTPUT;
          break;
        default:
          reqStat = PID_INVALID;
          break;
      }
      break;
    default: //invalid request
      reqStat = PID_INVALID;
      if(req->wLength > 0)
        USBD_CtlPrepareRx(&hUsbDeviceFS, NULL, req->wLength);
      break;
  }
  USBD_PID_ReqServ();
  return USBD_OK;
}
uint8_t USBD_PID_ReqServ()
{
  switch(reqStat){
    case PID_IDLE:
      break;
    case PID_INVALID:
      break;
    case PID_SET_FEATURE:
      USBD_CtlPrepareRx(&hUsbDeviceFS, HID_Out_Report, reportLen); //prepare receive
      break;
    case PID_SET_OUTPUT:
			//USBD_LL_PrepareReceive(&hUsbDeviceFS, HID_EPOUT_ADDR, HID_Out_Report, HID_EPOUT_SIZE); //prepare receive
      break;
    case PID_GET_FEATURE:
      FFBMngrFeatureServ(reportID, 1, NULL);
      break;
    case PID_GET_INPUT:
			FFBMngrDataInServ(reportID);
      break;
  }
	reqStat = PID_IDLE; //clear status
  return USBD_OK;
}
uint8_t USBD_PID_Init (USBD_HandleTypeDef *pdev)
{
  /*add FFBmngrInit to HID_Init*/
  FFBMngrInit();
  USBD_LL_PrepareReceive(&hUsbDeviceFS, HID_EPOUT_ADDR, HID_Out_Report, HID_EPOUT_SIZE);
  /*prepare first OUT pipe report*/
  return USBD_OK;
}
uint8_t USBD_PID_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	/*set EP1 NAK when processing dataout stage*/
	PCD_HandleTypeDef *hpcd =  (PCD_HandleTypeDef*) hUsbDeviceFS.pData;
	PCD_SET_EP_RX_STATUS(hpcd->Instance, PCD_ENDP2, USB_EP_RX_NAK); //beantowel
  /*EP1 out pipe data out Stage hook*/
  if(epnum == HID_EPOUT_ADDR) {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_11); //toggle PC11
    uint16_t len = USBD_GetRxCount(&hUsbDeviceFS, epnum);
    FFBMngrDataOutServ(HID_Out_Report, len);
  } else {
    USBD_ErrLog("Out on unexpected Endpoint");
  }
	/*clear EP1 NAK when processing dataout stage*/
	PCD_SET_EP_RX_STATUS(hpcd->Instance, PCD_ENDP2, USB_EP_RX_VALID); //beantowel
  /*prepare for next Output Report*/
  USBD_LL_PrepareReceive(&hUsbDeviceFS, HID_EPOUT_ADDR, HID_Out_Report, HID_EPOUT_SIZE);
  return USBD_OK;
}
uint8_t USBD_PID_EP0_RxReady (USBD_HandleTypeDef *pdev)
{
  /*control pipe receive ready hook*/
  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_11); //toggle PC11
  FFBMngrFeatureServ(HID_Out_Report[0], 0, HID_Out_Report); //host to device
  return USBD_OK;
}
uint8_t USBD_PID_Send (uint8_t *data, uint16_t len)
{
  /*Interface for ffb_manager*/
  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4);
  return USBD_HID_SendReport(&hUsbDeviceFS, data, len);
}
uint8_t USBD_PID_Send_EP0 (uint8_t *data, uint16_t len)
{
  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4);
  return USBD_CtlSendData (&hUsbDeviceFS, data, len);
}






















