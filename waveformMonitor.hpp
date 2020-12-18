
/*********************************************************************************************

    This is public domain software that was developed by or for the U.S. Naval Oceanographic
    Office and/or the U.S. Army Corps of Engineers.

    This is a work of the U.S. Government. In accordance with 17 USC 105, copyright protection
    is not available for any work of the U.S. Government.

    Neither the United States Government, nor any employees of the United States Government,
    nor the author, makes any warranty, express or implied, without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, or assumes any liability or
    responsibility for the accuracy, completeness, or usefulness of any information,
    apparatus, product, or process disclosed, or represents that its use would not infringe
    privately-owned rights. Reference herein to any specific commercial products, process,
    or service by trade name, trademark, manufacturer, or otherwise, does not necessarily
    constitute or imply its endorsement, recommendation, or favoring by the United States
    Government. The views and opinions of authors expressed herein do not necessarily state
    or reflect those of the United States Government, and shall not be used for advertising
    or product endorsement purposes.
*********************************************************************************************/


/****************************************  IMPORTANT NOTE  **********************************

    Comments in this file that start with / * ! or / / ! are being used by Doxygen to
    document the software.  Dashes in these comment blocks are used to create bullet lists.
    The lack of blank lines after a block of dash preceeded comments means that the next
    block of dash preceeded comments is a new, indented bullet list.  I've tried to keep the
    Doxygen formatting to a minimum but there are some other items (like <br> and <pre>)
    that need to be left alone.  If you see a comment that starts with / * ! or / / ! and
    there is something that looks a bit weird it is probably due to some arcane Doxygen
    syntax.  Be very careful modifying blocks of Doxygen comments.

*****************************************  IMPORTANT NOTE  **********************************/



/*  waveformMonitor class definitions.  */

#ifndef WAVEFORMMONITOR_H
#define WAVEFORMMONITOR_H

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <getopt.h>
#include <cmath>

#include "nvutility.h"
#include "nvutility.hpp"

#include "pfm.h"
#include "ABE.h"

#include "FileHydroOutput.h"
#include "FileTopoOutput.h"
#include "FileWave.h"

#include "czmil.h"

#include "version.hpp"


#include <QtCore>
#include <QtGui>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif


//  Savitzky-Golay

#define  NMAX  2048    /*  Maximum number of input data ordinates  */
#define  NP      50    /*  Maximum number of filter coefficients  */


#define WAVE_X_SIZE       270
#define WAVE_Y_SIZE       600
#define SPOT_SIZE         2


typedef struct
{
  int32_t             min_x;
  int32_t             max_x;
  int32_t             min_y;
  int32_t             max_y;
  int32_t             range_x;
  int32_t             range_y;
  int32_t             length;
  uint16_t            ac_zero_offset;
} BOUNDS;


typedef struct
{
  int32_t        length;
  int32_t        type;
  uint16_t       data[4][1024];       //  Overkill
  int32_t        chan;                //  CZMIL channel
  int32_t        ret;                 //  CZMIL return number
} WAVE;


class waveformMonitor:public QMainWindow
{
  Q_OBJECT 


public:

  waveformMonitor (int32_t *argc = 0, char **argv = 0, QWidget *parent = 0);
  ~waveformMonitor ();


protected:

  int32_t         key;

  QSharedMemory   *abeShare;

  ABE_SHARE       *abe_share, l_share;

  uint32_t        kill_switch;

  WAVE_HEADER_T   wave_header;
  BOUNDS          bounds[4];
  int32_t         width, height, window_x, window_y;

  char            wave_path[512], filename[512], progname[256];
  int32_t         flightline, recnum;
  HOF_HEADER_T    hof_header;
  HYDRO_OUTPUT_T  hof_record;
  CZMIL_CPF_Header cpf_header;
  CZMIL_CWF_Header cwf_header;
  CZMIL_CPF_Data  cpf_record;
  CZMIL_CWF_Data  cwf_record;
  uint8_t         secondary, wave_line_mode, startup_message, lock_track;
  float           tide;

  WAVE_DATA_T     wave_data;
  int32_t         wave_read;
  WAVE            wave;

  QMessageBox     *filError;

  QCheckBox       *sMessage;

  QStatusBar      *statusBar[5];

  QLabel          *dateLabel, *timestampLabel, *recordLabel, *correctDepth, *tideLabel, 
                  *dataType, *abdcLabel, *fullConfLabel, *nameLabel, 
                  *reportedDepth, *secDepth, *waveHeight, *sabdcLabel, *botBinLabel,
                  *secBotBinLabel, *sfcBinLabel, *fullConf, *secFullConf, *bfomThresh, 
                  *secBfomThresh, *sigStrength, *secSigStrength;

  QColor          waveColor[4], acZeroColor[4], surfaceColor, primaryColor, secondaryColor, backgroundColor;

  QPalette        bWavePalette[4], bSurfacePalette, bPrimaryPalette, bSecondaryPalette, bBackgroundPalette;

  QPushButton     *bWaveColor[4], *bSurfaceColor, *bPrimaryColor, *bSecondaryColor, *bBackgroundColor, *bRestoreDefaults; 

  uint8_t         force_redraw, display[4];

  nvMap           *map;

  NVMAP_DEF       mapdef;

  uint32_t        ac[7];

  QButtonGroup    *bGrp;
  QDialog         *prefsD;
  QToolButton     *bQuit, *bPrefs, *bMode;

  QString         pos_format, timestamp, record, first, level, water_level, sabdc,
                  pos_conf, flat, flon, sec_depth, slat, slon, ssig, s_full_conf,
                  f_full_conf, sfc_fom_a, sfc_fom_i, sfc_fom_r, fsig, bot_bin, haps_v,
                  sec_bot_bin, nadir_ang, scanner_az, mode, date_time, db_name, rep_depth,
                  wave_height, sfc_bin, sec_sfc_bin, calc_bfom, sec_calc_bfom, sug_stat, 
                  sus_stat, tide_stat, top, alt, warn, warn2, warn3, calc0, calc1, pri_run,
                  pri_slope, pri_backslope, sec_run, sec_slope, sec_backslope, parentName;

  int32_t         abdc;

  QFont           font;                       //  Font used for all ABE GUI applications


  //  Savitzky-Golay

  float           signal[NMAX + 1], ysave[NMAX + 1];
  float           coeff[NP + 1];
  int32_t         ndex[NP + 1], nleft, nright, moment, ndata;


  void envin ();
  void envout ();

  void leftMouse (double x, double y);
  void midMouse (double x, double y);
  void rightMouse (double x, double y);
  void scaleWave (int32_t x, int32_t y, int32_t *new_x, int32_t *new_y, int32_t type, NVMAP_DEF l_mapdef);
  void drawX (int32_t x, int32_t y, int32_t size, int32_t width, QColor color);
  void setFields ();


protected slots:

  void slotMousePress (QMouseEvent *e, double x, double y);
  void slotMouseMove (QMouseEvent *e, double x, double y);
  void slotResize (QResizeEvent *e);
  void closeEvent (QCloseEvent *event);
  void slotPlotWaves (NVMAP_DEF l_mapdef);

  void trackCursor ();

  void slotKeyPress (QKeyEvent *e);

  void slotHelp ();

  void slotQuit ();

  void slotMode (bool state);

  void slotPrefs ();
  void slotPosClicked (int id);
  void slotClosePrefs ();

  void slotWaveColor (int id);
  void slotSurfaceColor ();
  void slotPrimaryColor ();
  void slotSecondaryColor ();
  void slotBackgroundColor ();
  void slotRestoreDefaults ();

  void about ();
  void slotAcknowledgments ();
  void aboutQt ();


 private:
};

#endif
