#ifndef _MYMENU_H
#define _MYMENU_H

#define SENSORS 2
#define OUTPUTS 1

// define menu colors --------------------------------------------------------
//  {{disabled normal,disabled selected},{enabled normal,enabled selected, enabled editing}}
// monochromatic color table
#define ST77XX_GRAY RGB565(128,128,128)
const colorDef<uint16_t> colors[6] MEMMODE = {
    {{(uint16_t)ST77XX_BLACK, (uint16_t)ST77XX_BLACK}, {(uint16_t)ST77XX_BLACK, (uint16_t)ST77XX_GRAY, (uint16_t)ST77XX_RED}},     // bgColor
    {{(uint16_t)ST77XX_CYAN, (uint16_t)ST77XX_CYAN}, {(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_WHITE}},     // fgColor
    {{(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_BLACK}, {(uint16_t)ST77XX_YELLOW, (uint16_t)ST77XX_YELLOW, (uint16_t)ST77XX_RED}},   // valColor
    {{(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_BLACK}, {(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_YELLOW, (uint16_t)ST77XX_YELLOW}}, // unitColor
    {{(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_CYAN}, {(uint16_t)ST77XX_BLACK, (uint16_t)ST77XX_GRAY, (uint16_t)ST77XX_WHITE}},     // cursorColor
    {{(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_YELLOW}, {(uint16_t)ST77XX_WHITE, (uint16_t)ST77XX_BLACK, (uint16_t)ST77XX_BLACK}},       // titleColor
};

double_t SP1=0, SP2=0;
//altFIELD(decPlaces<3>::menuField,test2,"Test"," Ohm",98.000,102.000,0.1,0.001,doNothing,noEvent,wrapStyle),
// FIELD(test,"Power","mOhm",98000,102000,100,1,doNothing,noEvent,noStyle),

/*MENU(subMenuOpr, " Operateur", doNothing, noEvent, wrapStyle, 
  FIELD(board.reg[0].SP,"SP1:"," C",0,100,10,1,doNothing,noEvent,wrapStyle),
#if OUTPUTS >= 2
  FIELD(adc.reg[1].SP,"SP2:"," C",0,100,1,0.1,doNothing,noEvent,wrapStyle),
#endif
  EXIT("<Back"));


SELECT(opc.input.rtd[0].measurementType,selMenuConfInpType1,"Mesure",doNothing,noEvent,wrapStyle
  ,VALUE("2 fils",TYPE_2WIRE,doNothing,noEvent)
  ,VALUE("3 fils",TYPE_3WIRE,doNothing,noEvent)
  ,VALUE("4 fils",TYPE_4WIRE,doNothing,noEvent)
);
MENU(subMenuConfInp1, " Sonde 1", doNothing, noEvent, wrapStyle, 
  SUBMENU(selMenuConfInpType1),
  FIELD(opc.input.rtd[0].offset,"Offset:"," C",-5,5,1,0.1,doNothing,noEvent,wrapStyle),
  EXIT("<Back"));


#if SENSORS >= 2
SELECT(opc.input.rtd[1].measurementType,selMenuConfInpType2,"Mesure",doNothing,noEvent,wrapStyle
  ,VALUE("2 fils",TYPE_2WIRE,doNothing,noEvent)
  ,VALUE("3 fils",TYPE_3WIRE,doNothing,noEvent)
  ,VALUE("4 fils",TYPE_4WIRE,doNothing,noEvent)
);
MENU(subMenuConfInp2, " Sonde 2", doNothing, noEvent, wrapStyle, 
  SUBMENU(selMenuConfInpType2),
  FIELD(opc.input.rtd[1].offset,"Offset:"," C",-5,5,1,0.1,doNothing,noEvent,wrapStyle),
  EXIT("<Back"));
#endif
#if SENSORS >= 3
SELECT(adc.rtd[2].measurementType,selMenuConfInpType3,"Mesure",doNothing,noEvent,wrapStyle
  ,VALUE("2 fils",TYPE_2WIRE,doNothing,noEvent)
  ,VALUE("3 fils",TYPE_3WIRE,doNothing,noEvent)
  ,VALUE("4 fils",TYPE_4WIRE,doNothing,noEvent)
);
MENU(subMenuConfInp3, " Sonde 3", doNothing, noEvent, wrapStyle, 
  SUBMENU(selMenuConfInpType3),
  FIELD(adc.rtd[2].offset,"Offset:"," C",-5,5,1,0.1,doNothing,noEvent,wrapStyle),
  EXIT("<Back"));
#endif

MENU(subMenuConfInp, " Sondes", doNothing, noEvent, wrapStyle, 
  SUBMENU(subMenuConfInp1),
#if SENSORS >= 2
  SUBMENU(subMenuConfInp2),
#endif
#if SENSORS >= 3
  SUBMENU(subMenuConfInp3),
#endif
  EXIT("<Back"));



/*SELECT(opc.input.reg[0].mode,selMenuConfCntrSP1Mode,"Mode",doNothing,noEvent,wrapStyle
  ,VALUE("Chauffer",MODE_HEATING,doNothing,noEvent)
  ,VALUE("Refroidir",MODE_COOLING,doNothing,noEvent)
);
MENU(subMenuConfCntrSP1, " Sortie 1", doNothing, noEvent, wrapStyle, 
  SUBMENU(selMenuConfCntrSP1Mode),
  FIELD(opc.input.reg[0].hysteresis,"Hysteresis:"," C",0,10,1,0.1,doNothing,noEvent,wrapStyle),
  EXIT("<Back"));

#if OUTPUTS >= 2
SELECT(adc.reg[1].mode,selMenuConfCntrSP2Mode,"Mode",doNothing,noEvent,wrapStyle
  ,VALUE("Deshumidifier",1,doNothing,noEvent)
  ,VALUE("Humidifier",2,doNothing,noEvent)
);
MENU(subMenuConfCntrSP2, " Sortie 2", doNothing, noEvent, wrapStyle, 
  SUBMENU(selMenuConfCntrSP2Mode),
  FIELD(adc.reg[1].hysteresis,"Hysteresis:"," C",0,10,1,0.1,doNothing,noEvent,wrapStyle),
  EXIT("<Back"));
#endif

MENU(subMenuConfCntr, " Regulateur", doNothing, noEvent, wrapStyle,
     SUBMENU(subMenuConfCntrSP1),
#if OUTPUTS >= 2
     SUBMENU(subMenuConfCntrSP2),
#endif
     EXIT("<Back"));

MENU(subMenuConfCal, " Calibration", doNothing, noEvent, wrapStyle,
  OP(" ATTENTION !", doNothing,noEvent),
  altFIELD(decPlaces<3>::menuField,board.calResistanceValue," Rcal"," Ohm",99.000,101.000,0.1,0.001,doNothing,noEvent,wrapStyle),
  OP(" Calibrer", menuConfCal,enterEvent),
  EXIT("<Back"));

MENU(subMenuConf, " Configuration", doNothing, noEvent, wrapStyle, 
  SUBMENU(subMenuConfInp), 
  SUBMENU(subMenuConfCntr),
  SUBMENU(subMenuConfCal),
  EXIT("<Back"));


MENU(mainMenu, " Menu principal", doNothing, noEvent, wrapStyle, 
  //SUBMENU(subMenuOpr),
  //SUBMENU(subMenuConf),
  EXIT("<Back"));*/



#endif // _MYMENU_H