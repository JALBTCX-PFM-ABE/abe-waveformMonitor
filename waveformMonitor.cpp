
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



#include "waveformMonitor.hpp"
#include "waveformMonitorHelp.hpp"
#include "acknowledgments.hpp"


double settings_version = 2.01;


waveformMonitor::waveformMonitor (int32_t *argc, char **argv, QWidget * parent):
  QMainWindow (parent, 0)
{
  extern char     *optarg;


  strcpy (progname, argv[0]);
  filError = NULL;
  lock_track = NVFalse;


  QResource::registerResource ("/icons.rcc");


  //  Have to set the focus policy or keypress events don't work properly at first in Focus Follows Mouse mode

  setFocusPolicy (Qt::WheelFocus);


  //  Set the main icon

  setWindowIcon (QIcon (":/icons/waveform_monitor.png"));


  kill_switch = ANCILLARY_FORCE_EXIT;

  int32_t option_index = 0;
  while (NVTrue) 
    {
      static struct option long_options[] = {{"actionkey00", required_argument, 0, 0},
                                             {"actionkey01", required_argument, 0, 0},
                                             {"actionkey02", required_argument, 0, 0},
                                             {"actionkey03", required_argument, 0, 0},
                                             {"shared_memory_key", required_argument, 0, 0},
                                             {"kill_switch", required_argument, 0, 0},
                                             {0, no_argument, 0, 0}};

      char c = (char) getopt_long (*argc, argv, "o", long_options, &option_index);
      if (c == -1) break;

      switch (c) 
        {
        case 0:

          //  The parent ID argument

          switch (option_index)
            {
            case 4:
              sscanf (optarg, "%d", &key);
              break;

            case 5:
              sscanf (optarg, "%d", &kill_switch);
              break;

            default:
              char tmp;
              sscanf (optarg, "%1c", &tmp);
              ac[option_index] = (uint32_t) tmp;
              break;
            }
          break;
        }
    }


  //  This is the "tools" toolbar.  We have to do this here so that we can restore the toolbar location(s).

  QToolBar *tools = addToolBar (tr ("Tools"));
  tools->setObjectName (tr ("waveformMonitor main toolbar"));


  envin ();


  // Set the application font

  QApplication::setFont (font);


  setWindowTitle (QString (VERSION));


  /******************************************* IMPORTANT NOTE ABOUT SHARED MEMORY **************************************** \

      This is a little note about the use of shared memory within the Area-Based Editor (ABE) programs.  If you read
      the Qt documentation (or anyone else's documentation) about the use of shared memory they will say "Dear [insert
      name of omnipotent being of your choice here], whatever you do, always lock shared memory when you use it!".
      The reason they say this is that access to shared memory is not atomic.  That is, reading shared memory and then
      writing to it is not a single operation.  An example of why this might be important - two programs are running,
      the first checks a value in shared memory, sees that it is a zero.  The second program checks the same location
      and sees that it is a zero.  These two programs have different actions they must perform depending on the value
      of that particular location in shared memory.  Now the first program writes a one to that location which was
      supposed to tell the second program to do something but the second program thinks it's a zero.  The second program
      doesn't do what it's supposed to do and it writes a two to that location.  The two will tell the first program 
      to do something.  Obviously this could be a problem.  In real life, this almost never occurs.  Also, if you write
      your program properly you can make sure this doesn't happen.  In ABE we almost never lock shared memory because
      something much worse than two programs getting out of sync can occur.  If we start a program and it locks shared
      memory and then dies, all the other programs will be locked up.  When you look through the ABE code you'll see
      that we very rarely lock shared memory, and then only for very short periods of time.  This is by design.

  \******************************************* IMPORTANT NOTE ABOUT SHARED MEMORY ****************************************/


  //  Get the shared memory area.  If it doesn't exist, quit.  It should have already been created 
  //  by pfmView or fileEdit3D.

  QString skey;
  skey.sprintf ("%d_abe", key);

  abeShare = new QSharedMemory (skey);

  if (!abeShare->attach (QSharedMemory::ReadWrite))
    {
      fprintf (stderr, "%s %s %s %d - abeShare - %s\n", progname, __FILE__, __FUNCTION__, __LINE__, strerror (errno));
      exit (-1);
    }

  abe_share = (ABE_SHARE *) abeShare->data ();


  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //  Just playing with the Savitzky-Golay filter in the following block.  I'm going to leave it here in case anyone wants to
  //  play around with it.  The overhead is pretty much non-existent.

  for (int32_t i = 1 ; i <= NMAX ; i++) signal[i] = ysave[i] = 0.0;
  nleft = 4;
  nright = 4;
  moment = 2;

  ndex[1] = 0;
  int32_t j = 3;
  for (int32_t i = 2 ; i <= nleft + 1 ; i++)
    {
      ndex[i] = i - j;
      j += 2;
    }

  j = 2;
  for (int32_t i = nleft + 2 ; i <= nleft + nright + 1 ; i++)
    {
      ndex[i] = i - j;
      j += 2;
    }


  //  Calculate Savitzky-Golay filter coefficients.

  savgol (coeff, nleft + nright + 1, nleft, nright, 0, moment);

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  //  Set the window size and location from the defaults

  this->resize (width, height);
  this->move (window_x, window_y);


  //  Set all of the default values.

  wave_read = 0;


  //  Set the map values from the defaults

  mapdef.projection = NO_PROJECTION;
  mapdef.draw_width = width;
  mapdef.draw_height = height;
  mapdef.overlap_percent = 5;
  mapdef.grid_inc_x = 0.0;
  mapdef.grid_inc_y = 50.0;

  mapdef.coasts = NVFalse;
  mapdef.landmask = NVFalse;

  mapdef.border = 0;
  mapdef.coast_color = Qt::white;
  mapdef.grid_color = QColor (160, 160, 160, 127);
  mapdef.background_color = backgroundColor;


  mapdef.initial_bounds.min_x = 0;
  mapdef.initial_bounds.min_y = 0;
  mapdef.initial_bounds.max_x = 300;
  mapdef.initial_bounds.max_y = 500;


  QFrame *frame = new QFrame (this, 0);

  setCentralWidget (frame);


  //  Make the map.

  map = new nvMap (this, &mapdef);
  map->setWhatsThis (mapText);


  //  Connect to the signals from the map class.

  connect (map, SIGNAL (mousePressSignal (QMouseEvent *, double, double)), this, SLOT (slotMousePress (QMouseEvent *, double, double)));
  connect (map, SIGNAL (mouseMoveSignal (QMouseEvent *, double, double)), this, SLOT (slotMouseMove (QMouseEvent *, double, double)));
  connect (map, SIGNAL (resizeSignal (QResizeEvent *)), this, SLOT (slotResize (QResizeEvent *)));
  connect (map, SIGNAL (postRedrawSignal (NVMAP_DEF)), this, SLOT (slotPlotWaves (NVMAP_DEF)));


  //  Layouts, what fun!

  QVBoxLayout *vBox = new QVBoxLayout (frame);


  vBox->addWidget (map);


  for (int32_t i = 0 ; i < 5 ; i++)
    {
      statusBar[i] = new QStatusBar (frame);
      statusBar[i]->setSizeGripEnabled (false);
      statusBar[i]->show ();
      vBox->addWidget (statusBar[i]);
    }


  nameLabel = new QLabel ("                                        ", this);
  nameLabel->setAlignment (Qt::AlignLeft);
  nameLabel->setMinimumSize (nameLabel->sizeHint ());
  nameLabel->setWhatsThis (nameLabelText);
  nameLabel->setToolTip (tr ("File name"));

  dateLabel = new QLabel ("0000-00-00 (000) 00:00:00.00", this);
  dateLabel->setAlignment (Qt::AlignCenter);
  dateLabel->setMinimumSize (dateLabel->sizeHint ());
  dateLabel->setWhatsThis (dateLabelText);
  dateLabel->setToolTip (tr ("Date and time"));

  recordLabel = new QLabel ("00000000", this);
  recordLabel->setAlignment (Qt::AlignCenter);
  recordLabel->setMinimumSize (recordLabel->sizeHint ());
  recordLabel->setWhatsThis (recordLabelText);
  recordLabel->setToolTip (tr ("Record number"));

  dataType = new QLabel ("KGPS", this);
  dataType->setAlignment (Qt::AlignCenter);
  dataType->setMinimumSize (dataType->sizeHint ());
  dataType->setWhatsThis (dataTypeText);
  dataType->setToolTip (tr ("Data type"));

  correctDepth = new QLabel ("0000.00", this);
  correctDepth->setAlignment (Qt::AlignCenter);
  correctDepth->setMinimumSize (correctDepth->sizeHint ());
  correctDepth->setWhatsThis (correctDepthText);
  correctDepth->setToolTip (tr ("Tide/datum corrected depth"));

  secDepth = new QLabel ("0000.00", this);
  secDepth->setAlignment (Qt::AlignCenter);
  secDepth->setMinimumSize (secDepth->sizeHint ());
  secDepth->setWhatsThis (secDepthText);
  secDepth->setToolTip (tr ("Tide/datum corrected second depth"));

  reportedDepth = new QLabel ("0000.00", this);
  reportedDepth->setAlignment (Qt::AlignCenter);
  reportedDepth->setMinimumSize (reportedDepth->sizeHint ());
  reportedDepth->setWhatsThis (reportedDepthText);
  reportedDepth->setToolTip (tr ("Uncorrected depth"));

  tideLabel = new QLabel ("0000.00", this);
  tideLabel->setAlignment (Qt::AlignCenter);
  tideLabel->setMinimumSize (tideLabel->sizeHint ());
  tideLabel->setWhatsThis (tideLabelText);
  tideLabel->setToolTip (tr ("Tide value"));

  waveHeight = new QLabel ("000.00", this);
  waveHeight->setAlignment (Qt::AlignCenter);
  waveHeight->setMinimumSize (waveHeight->sizeHint ());
  waveHeight->setWhatsThis (waveHeightText);
  waveHeight->setToolTip (tr ("Wave height"));

  abdcLabel = new QLabel ("00", this);
  abdcLabel->setAlignment (Qt::AlignCenter);
  abdcLabel->setMinimumSize (abdcLabel->sizeHint ());
  abdcLabel->setWhatsThis (abdcLabelText);
  abdcLabel->setToolTip (tr ("Abbreviated depth confidence"));

  sabdcLabel = new QLabel ("N/A", this);
  sabdcLabel->setAlignment (Qt::AlignCenter);
  sabdcLabel->setMinimumSize (sabdcLabel->sizeHint ());
  sabdcLabel->setWhatsThis (sabdcLabelText);
  sabdcLabel->setToolTip (tr ("Second abbreviated depth confidence"));

  botBinLabel = new QLabel ("Shallow 000", this);
  botBinLabel->setAlignment (Qt::AlignCenter);
  botBinLabel->setMinimumSize (botBinLabel->sizeHint ());
  botBinLabel->setWhatsThis (botBinLabelText);
  botBinLabel->setToolTip (tr ("Bottom channel and bin used"));

  secBotBinLabel = new QLabel ("Deep 000", this);
  secBotBinLabel->setAlignment (Qt::AlignCenter);
  secBotBinLabel->setMinimumSize (secBotBinLabel->sizeHint ());
  secBotBinLabel->setWhatsThis (secBotBinLabelText);
  secBotBinLabel->setToolTip (tr ("Second bottom channel and bin used"));

  sfcBinLabel = new QLabel ("Raman 000", this);
  sfcBinLabel->setAlignment (Qt::AlignCenter);
  sfcBinLabel->setMinimumSize (sfcBinLabel->sizeHint ());
  sfcBinLabel->setWhatsThis (sfcBinLabelText);
  sfcBinLabel->setToolTip (tr ("Surface channel and bin used"));

  fullConf = new QLabel ("000000", this);
  fullConf->setAlignment (Qt::AlignCenter);
  fullConf->setMinimumSize (fullConf->sizeHint ());
  fullConf->setWhatsThis (fullConfText);
  fullConf->setToolTip (tr ("Full confidence value"));

  secFullConf = new QLabel ("000000", this);
  secFullConf->setAlignment (Qt::AlignCenter);
  secFullConf->setMinimumSize (secFullConf->sizeHint ());
  secFullConf->setWhatsThis (secFullConfText);
  secFullConf->setToolTip (tr ("Second full confidence value"));

  bfomThresh = new QLabel ("00", this);
  bfomThresh->setAlignment (Qt::AlignCenter);
  bfomThresh->setMinimumSize (bfomThresh->sizeHint ());
  bfomThresh->setWhatsThis (bfomThreshText);
  bfomThresh->setToolTip (tr ("Bottom figure of merit threshold * 10"));

  secBfomThresh = new QLabel ("00", this);
  secBfomThresh->setAlignment (Qt::AlignCenter);
  secBfomThresh->setMinimumSize (secBfomThresh->sizeHint ());
  secBfomThresh->setWhatsThis (secBfomThreshText);
  secBfomThresh->setToolTip (tr ("Second bottom figure of merit threshold * 10"));

  sigStrength = new QLabel ("0000.0", this);
  sigStrength->setAlignment (Qt::AlignCenter);
  sigStrength->setMinimumSize (sigStrength->sizeHint ());
  sigStrength->setWhatsThis (sigStrengthText);
  sigStrength->setToolTip (tr ("Shallow bottom signal strength"));

  secSigStrength = new QLabel ("0000.0", this);
  secSigStrength->setAlignment (Qt::AlignCenter);
  secSigStrength->setMinimumSize (secSigStrength->sizeHint ());
  secSigStrength->setWhatsThis (secSigStrengthText);
  secSigStrength->setToolTip (tr ("Deep bottom signal strength"));


  statusBar[0]->addWidget (nameLabel, 1);
  statusBar[1]->addWidget (dateLabel);
  statusBar[1]->addWidget (dataType);
  statusBar[1]->addWidget (recordLabel);
  statusBar[2]->addWidget (correctDepth);
  statusBar[2]->addWidget (secDepth);
  statusBar[2]->addWidget (reportedDepth);
  statusBar[2]->addWidget (tideLabel);
  statusBar[2]->addWidget (waveHeight);
  statusBar[3]->addWidget (abdcLabel);
  statusBar[3]->addWidget (sabdcLabel);
  statusBar[3]->addWidget (botBinLabel);
  statusBar[3]->addWidget (secBotBinLabel);
  statusBar[3]->addWidget (sfcBinLabel);
  statusBar[4]->addWidget (fullConf);
  statusBar[4]->addWidget (secFullConf);
  statusBar[4]->addWidget (bfomThresh);
  statusBar[4]->addWidget (secBfomThresh);
  statusBar[4]->addWidget (sigStrength);
  statusBar[4]->addWidget (secSigStrength);


  //  Button, button, who's got the buttons?

  bQuit = new QToolButton (this);
  bQuit->setIcon (QIcon (":/icons/quit.png"));
  bQuit->setToolTip (tr ("Quit"));
  bQuit->setWhatsThis (quitText);
  connect (bQuit, SIGNAL (clicked ()), this, SLOT (slotQuit ()));
  tools->addWidget (bQuit);


  bMode = new QToolButton (this);
  if (wave_line_mode)
    {
      bMode->setIcon (QIcon (":/icons/mode_line.png"));
    }
  else
    {
      bMode->setIcon (QIcon (":/icons/mode_dot.png"));
    }
  bMode->setToolTip (tr ("Wave drawing mode toggle"));
  bMode->setWhatsThis (modeText);
  bMode->setCheckable (true);
  bMode->setChecked (wave_line_mode);
  connect (bMode, SIGNAL (toggled (bool)), this, SLOT (slotMode (bool)));
  tools->addWidget (bMode);


  bPrefs = new QToolButton (this);
  bPrefs->setIcon (QIcon (":/icons/prefs.png"));
  bPrefs->setToolTip (tr ("Change application preferences"));
  bPrefs->setWhatsThis (prefsText);
  connect (bPrefs, SIGNAL (clicked ()), this, SLOT (slotPrefs ()));
  tools->addWidget (bPrefs);


  tools->addSeparator ();
  tools->addSeparator ();


  QAction *bHelp = QWhatsThis::createAction (this);
  bHelp->setIcon (QIcon (":/icons/contextHelp.png"));
  tools->addAction (bHelp);



  //  Setup the file menu.

  QAction *fileQuitAction = new QAction (tr ("&Quit"), this);
  fileQuitAction->setShortcut (tr ("Ctrl+Q"));
  fileQuitAction->setStatusTip (tr ("Exit from application"));
  connect (fileQuitAction, SIGNAL (triggered ()), this, SLOT (slotQuit ()));


  QMenu *fileMenu = menuBar ()->addMenu (tr ("&File"));
  fileMenu->addAction (fileQuitAction);


  //  Setup the help menu.

  QAction *aboutAct = new QAction (tr ("&About"), this);
  aboutAct->setShortcut (tr ("Ctrl+A"));
  aboutAct->setStatusTip (tr ("Information about waveformMonitor"));
  connect (aboutAct, SIGNAL (triggered ()), this, SLOT (about ()));

  QAction *acknowledgments = new QAction (tr ("A&cknowledgments"), this);
  acknowledgments->setShortcut (tr ("Ctrl+c"));
  acknowledgments->setStatusTip (tr ("Information about supporting libraries"));
  connect (acknowledgments, SIGNAL (triggered ()), this, SLOT (slotAcknowledgments ()));

  QAction *aboutQtAct = new QAction (tr ("About Qt"), this);
  aboutQtAct->setStatusTip (tr ("Information about Qt"));
  connect (aboutQtAct, SIGNAL (triggered ()), this, SLOT (aboutQt ()));

  QMenu *helpMenu = menuBar ()->addMenu (tr ("&Help"));
  helpMenu->addAction (aboutAct);
  helpMenu->addSeparator ();
  helpMenu->addAction (acknowledgments);
  helpMenu->addAction (aboutQtAct);


  map->setCursor (Qt::ArrowCursor);

  map->enableSignals ();


  QTimer *track = new QTimer (this);
  connect (track, SIGNAL (timeout ()), this, SLOT (trackCursor ()));
  track->start (10);
}



waveformMonitor::~waveformMonitor ()
{
}



void 
waveformMonitor::closeEvent (QCloseEvent *event __attribute__ ((unused)))
{
  slotQuit ();
}



void 
waveformMonitor::slotResize (QResizeEvent *e __attribute__ ((unused)))
{
  force_redraw = NVTrue;
}



void
waveformMonitor::slotHelp ()
{
  QWhatsThis::enterWhatsThisMode ();
}



void 
waveformMonitor::leftMouse (double x __attribute__ ((unused)), double y __attribute__ ((unused)))
{
  //  Placeholder
}



void 
waveformMonitor::midMouse (double x __attribute__ ((unused)), double y __attribute__ ((unused)))
{
  //  Placeholder
}



void 
waveformMonitor::rightMouse (double x __attribute__ ((unused)), double y __attribute__ ((unused)))
{
  //  Placeholder
}



//  Signal from the map class.

void 
waveformMonitor::slotMousePress (QMouseEvent * e, double x, double y)
{
  if (e->button () == Qt::LeftButton) leftMouse (x, y);
  if (e->button () == Qt::MidButton) midMouse (x, y);
  if (e->button () == Qt::RightButton) rightMouse (x, y);
}



//  Signal from the map class.

void
waveformMonitor::slotMouseMove (QMouseEvent *e __attribute__ ((unused)), double x __attribute__ ((unused)),
                            double y __attribute__ ((unused)))
{
  //  Placeholder
}



//  Timer - timeout signal.  Very much like an X workproc.

void
waveformMonitor::trackCursor ()
{
  int32_t                 year, day, mday, month, hour, minute;
  int32_t                 cpf_handle, cwf_handle, return_type = 0;
  float                   second;
  static uint32_t         prev_rec = UINT32_MAX;
  static int32_t          prev_sub = -1;
  static ABE_SHARE        l_share;
  static uint8_t          first = NVTrue;
  FILE                    *hfp = NULL, *wfp = NULL;


  //  Since this is always a child process of something we want to exit if we see the CHILD_PROCESS_FORCE_EXIT key.
  //  We also want to exit on the ANCILLARY_FORCE_EXIT key (from pfmEdit) or if our own personal kill signal
  //  has been placed in abe_share->key.

  if (abe_share->key == CHILD_PROCESS_FORCE_EXIT || abe_share->key == ANCILLARY_FORCE_EXIT ||
      abe_share->key == kill_switch) slotQuit ();


  //  We want to exit if we have locked the tracker to update our saved waveforms (in slotPlotWaves).

  if (lock_track) return;


  //  Locking makes sure another process does not have memory locked.  It will block until it can lock it.
  //  At that point we copy the contents and then unlock it so other processes can continue.

  abeShare->lock ();


  //  Check for change of record and correct record type (or change of record or subrecord for CZMIL data).

  uint8_t hit = NVFalse;
  if ((prev_rec != abe_share->mwShare.multiRecord[0] && (abe_share->mwShare.multiType[0] == PFM_SHOALS_1K_DATA ||
                                                         abe_share->mwShare.multiType[0] == PFM_CHARTS_HOF_DATA)) ||
      (abe_share->mwShare.multiType[0] == PFM_CZMIL_DATA && (prev_rec != abe_share->mwShare.multiRecord[0] ||
                                                             prev_sub != abe_share->mwShare.multiSubrecord[0])))
    {
      l_share = *abe_share;
      prev_rec = l_share.mwShare.multiRecord[0];
      prev_sub = l_share.mwShare.multiSubrecord[0];
      hit = NVTrue;
      return_type = abe_share->mwShare.multiType[0];
    }


  //  Check for action keys.

  if (abe_share->key == ac[0])
    {
      abe_share->key = 0;
      abe_share->modcode = NO_ACTION_REQUIRED;

      if (display[APD])
        {
          display[APD] = NVFalse;
        }
      else
        {
          display[APD] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (abe_share->key == ac[1])
    {
      abe_share->key = 0;
      abe_share->modcode = NO_ACTION_REQUIRED;

      if (display[PMT])
        {
          display[PMT] = NVFalse;
        }
      else
        {
          display[PMT] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (abe_share->key == ac[2])
    {
      abe_share->key = 0;
      abe_share->modcode = NO_ACTION_REQUIRED;

      if (display[IR])
        {
          display[IR] = NVFalse;
        }
      else
        {
          display[IR] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (abe_share->key == ac[3])
    {
      abe_share->key = 0;
      abe_share->modcode = NO_ACTION_REQUIRED;

      if (display[RAMAN])
        {
          display[RAMAN] = NVFalse;
        }
      else
        {
          display[RAMAN] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (abe_share->modcode == WAVEMONITOR_FORCE_REDRAW) force_redraw = NVTrue;


  abeShare->unlock ();


  //  Hit or force redraw from above.

  if (hit || force_redraw)
    {
      force_redraw = NVFalse;

 
      //  Save for slotPlotWaves.

      recnum = l_share.mwShare.multiRecord[0];
      strcpy (filename, l_share.nearest_filename);


      wave.type = l_share.mwShare.multiType[0];


      //  Open the HOF or CZMIL file and read the data.

      char string[512];

      if (l_share.mwShare.multiType[0] == PFM_SHOALS_1K_DATA || l_share.mwShare.multiType[0] == PFM_CHARTS_HOF_DATA)
        {
          strcpy (string, l_share.nearest_filename);
          db_name.sprintf ("%s", gen_basename (l_share.nearest_filename));


          strcpy (string, l_share.nearest_filename);
          sprintf (&string[strlen (string) - 4], ".inh");
          strcpy (wave_path, string);


          hfp = open_hof_file (l_share.nearest_filename);

          if (hfp == NULL)
            {
              if (filError) filError->close ();
              filError = new QMessageBox (QMessageBox::Warning, tr ("Open HOF file"), tr ("Error opening %1\n%2").arg
                                          (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                          (Qt::WindowFlags) Qt::WA_DeleteOnClose);
              filError->show ();
              return;
            }


          wfp = open_wave_file (wave_path);

          if (wfp == NULL)
            {
              if (filError) filError->close ();
              filError = new QMessageBox (QMessageBox::Warning, tr ("Open INH file"), tr ("Error opening %1\n%2").arg 
                                          (QDir::toNativeSeparators (QString (wave_path))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                          (Qt::WindowFlags) Qt::WA_DeleteOnClose);
              filError->show ();

              fclose (hfp);
              return;
            }


          wave_read_header (wfp, &wave_header);


          hof_read_header (hfp, &hof_header);
          hof_read_record (hfp, l_share.mwShare.multiRecord[0], &hof_record);


          wave_read = wave_read_record (wfp, l_share.mwShare.multiRecord[0], &wave_data);


          fclose (hfp);
          fclose (wfp);


          bounds[PMT].ac_zero_offset = wave_header.ac_zero_offset[PMT];
          bounds[APD].ac_zero_offset = wave_header.ac_zero_offset[APD];
          bounds[IR].ac_zero_offset = wave_header.ac_zero_offset[IR];
          bounds[RAMAN].ac_zero_offset = wave_header.ac_zero_offset[RAMAN];

          bounds[PMT].length = wave_header.pmt_size - 1;
          bounds[APD].length = wave_header.apd_size - 1;
          bounds[IR].length = wave_header.ir_size - 1;
          bounds[RAMAN].length = wave_header.raman_size - 1;


          for (int32_t j = 0 ; j < bounds[PMT].length ; j++) wave.data[PMT][j] = wave_data.pmt[j + 1];
          for (int32_t j = 0 ; j < bounds[APD].length ; j++) wave.data[APD][j] = wave_data.apd[j + 1];
          for (int32_t j = 0 ; j < bounds[IR].length ; j++) wave.data[IR][j] = wave_data.ir[j + 1];
          for (int32_t j = 0 ; j < bounds[RAMAN].length ; j++) wave.data[RAMAN][j] = wave_data.raman[j + 1];


          if (hof_record.sec_depth == -998.0) 
            {
              secondary = NVFalse;
            }
          else
            {
              secondary = NVTrue;
            }


          tide = 0.0;
          if (!hof_record.data_type && hof_record.correct_depth != -998.0)
            tide = -hof_record.correct_depth - hof_record.reported_depth;


          charts_cvtime (hof_record.timestamp, &year, &day, &hour, &minute, &second);
          charts_jday2mday (year, day, &month, &mday);
          month++;



          date_time = tr ("%1-%2-%3 (%4) %5:%6:%L7<br>").arg (year + 1900).arg (month, 2, 10, QChar ('0')).arg (mday, 2, 10, QChar ('0')).arg
            (day, 3, 10, QChar ('0')).arg (hour, 2, 10, QChar ('0')).arg (minute, 2, 10, QChar ('0')).arg (second, 5, 'f', 2, QChar ('0'));


          //  Set the min and max values.

          bounds[PMT].min_y = 0;
          bounds[PMT].max_y = 256;
          bounds[PMT].min_x = 0;
          bounds[PMT].max_x = bounds[PMT].length;

          bounds[APD].min_y = 0;
          bounds[APD].max_y = 256;

          bounds[IR].min_y = 0;
          bounds[IR].max_y = 256;

          bounds[RAMAN].min_y = 0;
          bounds[RAMAN].max_y = 256;


          //  Add 5% to the X axis.

          bounds[PMT].range_x = bounds[PMT].max_x - bounds[PMT].min_x;
          bounds[PMT].min_x = bounds[PMT].min_x - NINT (((float) bounds[PMT].range_x * 0.05 + 1));
          bounds[PMT].max_x = bounds[PMT].max_x + NINT (((float) bounds[PMT].range_x * 0.05 + 1));
          bounds[PMT].range_x = bounds[PMT].max_x - bounds[PMT].min_x;
          bounds[PMT].range_y = bounds[PMT].max_y - bounds[PMT].min_y;

          bounds[APD].range_y = bounds[APD].max_y - bounds[APD].min_y;

          bounds[IR].range_y = bounds[IR].max_y - bounds[IR].min_y;

          bounds[RAMAN].range_y = bounds[RAMAN].max_y - bounds[RAMAN].min_y;
        }
      else if (l_share.mwShare.multiType[0] == PFM_CZMIL_DATA)
        {
          strcpy (string, l_share.nearest_filename);
          db_name.sprintf ("%s", gen_basename (l_share.nearest_filename));


          strcpy (string, l_share.nearest_filename);
          sprintf (&string[strlen (string) - 4], ".cwf");
          strcpy (wave_path, string);


          if ((cpf_handle = czmil_open_cpf_file (l_share.nearest_filename, &cpf_header, CZMIL_READONLY)) < 0)
            {
              if (filError) filError->close ();
              filError = new QMessageBox (QMessageBox::Warning, tr ("Open CPF file"), tr ("Error opening %1\n%2").arg 
                                          (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (czmil_strerror ()), QMessageBox::NoButton, this, 
                                          (Qt::WindowFlags) Qt::WA_DeleteOnClose);
              filError->show ();
              return;
            }


          if ((cwf_handle = czmil_open_cwf_file (wave_path, &cwf_header, CZMIL_READONLY)) < 0)
            {
              if (filError) filError->close ();
              filError = new QMessageBox (QMessageBox::Warning, tr ("Open CWF file"), tr ("Error opening %1\n%2").arg 
                                          (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (czmil_strerror ()), QMessageBox::NoButton, this, 
                                          (Qt::WindowFlags) Qt::WA_DeleteOnClose);
              filError->show ();
              return;
            }


          czmil_read_cpf_record (cpf_handle, l_share.mwShare.multiRecord[0], &cpf_record);
          czmil_read_cwf_record (cwf_handle, l_share.mwShare.multiRecord[0], &cwf_record);
          wave_read = 1;


          czmil_close_cpf_file (cpf_handle);
          czmil_close_cwf_file (cwf_handle);


          wave.chan = l_share.mwShare.multiSubrecord[0] / 100;
          wave.ret = l_share.mwShare.multiSubrecord[0] % 100;


          bounds[PMT].length = cwf_record.number_of_packets[CZMIL_DEEP_CHANNEL] * 64;
          bounds[APD].length = cwf_record.number_of_packets[wave.chan] * 64;
          bounds[IR].length = cwf_record.number_of_packets[CZMIL_IR_CHANNEL] * 64;


          //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

          /*  Savitzky-Golay filter (just for fun).

          for (int32_t k = 0 ; k < bounds[PMT].length ; k++) signal[k + 1] = ysave[k + 1] = (float) cwf_record.channel[CZMIL_DEEP_CHANNEL][k];

          for (int32_t k = 1 ; k <= bounds[PMT].length - nright ; k++)
            {
              signal[k] = 0.0;
              for (int32_t m = 1 ; m <= nleft + nright + 1 ; m++)
                {
                  if (k + ndex[m] > 0)
                    {
                      signal[k] += coeff[m] * ysave[k + ndex[m]];
                    }
                }
            }

          for (int32_t k = 0 ; k < bounds[PMT].length ; k++)
            {
              cwf_record.channel[CZMIL_DEEP_CHANNEL][k] = NINT (signal[k + 1]);


              //  Clamp the signal to the limits of the window

              if (NINT (signal[k + 1]) < 0)
                {
                  cwf_record.channel[CZMIL_DEEP_CHANNEL][k] = 0;
                }
              else if (NINT (signal[k + 1]) > 1023)
                {
                  cwf_record.channel[CZMIL_DEEP_CHANNEL][k] = 1023;
                }
            }


          for (int32_t k = 0 ; k < bounds[APD].length ; k++) signal[k + 1] = ysave[k + 1] = (float) cwf_record.channel[wave.chan][k];

          for (int32_t k = 1 ; k <= bounds[APD].length - nright ; k++)
            {
              signal[k] = 0.0;
              for (int32_t m = 1 ; m <= nleft + nright + 1 ; m++)
                {
                  if (k + ndex[m] > 0)
                    {
                      signal[k] += coeff[m] * ysave[k + ndex[m]];
                    }
                }
            }

          for (int32_t k = 0 ; k < bounds[APD].length ; k++)
            {
              cwf_record.channel[wave.chan][k] = NINT (signal[k + 1]);


              //  Clamp the signal to the limits of the window

              if (NINT (signal[k + 1]) < 0)
                {
                  cwf_record.channel[wave.chan][k] = 0;
                }
              else if (NINT (signal[k + 1]) > 1023)
                {
                  cwf_record.channel[wave.chan][k] = 1023;
                }
            }

          //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
          */

          for (int32_t j = 0 ; j < bounds[PMT].length ; j++) wave.data[PMT][j] = cwf_record.channel[CZMIL_DEEP_CHANNEL][j];
          for (int32_t j = 0 ; j < bounds[APD].length ; j++) wave.data[APD][j] = cwf_record.channel[wave.chan][j];
          for (int32_t j = 0 ; j < bounds[IR].length ; j++) wave.data[IR][j] = cwf_record.channel[CZMIL_IR_CHANNEL][j];


          secondary = NVFalse;


          czmil_cvtime (cpf_record.timestamp, &year, &day, &hour, &minute, &second);
          czmil_jday2mday (year, day, &month, &mday);
          month++;


          date_time = tr ("%1-%2-%3 (%4) %5:%6:%L7<br>").arg (year + 1900).arg (month, 2, 10, QChar ('0')).arg (mday, 2, 10, QChar ('0')).arg
            (day, 3, 10, QChar ('0')).arg (hour, 2, 10, QChar ('0')).arg (minute, 2, 10, QChar ('0')).arg (second, 5, 'f', 2, QChar ('0'));


          //  Set the min and max values.

          bounds[PMT].min_y = 0;
          bounds[PMT].max_y = 1024;
          bounds[PMT].min_x = 0;
          bounds[PMT].max_x = bounds[PMT].length;

          bounds[APD].min_y = 0;
          bounds[APD].max_y = 1024;

          bounds[IR].min_y = 0;
          bounds[IR].max_y = 1024;


          //  Add 5% to the X axis.

          bounds[PMT].range_x = bounds[PMT].max_x - bounds[PMT].min_x;
          bounds[PMT].min_x = bounds[PMT].min_x - NINT (((float) bounds[PMT].range_x * 0.05 + 1));
          bounds[PMT].max_x = bounds[PMT].max_x + NINT (((float) bounds[PMT].range_x * 0.05 + 1));
          bounds[PMT].range_x = bounds[PMT].max_x - bounds[PMT].min_x;
          bounds[PMT].range_y = bounds[PMT].max_y - bounds[PMT].min_y;

          bounds[APD].range_y = bounds[APD].max_y - bounds[APD].min_y;

          bounds[IR].range_y = bounds[IR].max_y - bounds[IR].min_y;
        }
      else
        {
          return;
        }


      l_share.key = 0;
      abe_share->key = 0;
      abe_share->modcode = return_type;


      map->redrawMapArea (NVTrue);
    }


  //  Display the startup info message the first time through.

  if (first)
    {
      QString startupMessageText = 
        tr ("The following action keys are available in %1:\n\n"
            "%2 = Toggle display of the shallow waveform\n"
            "%3 = Toggle display of the deep waveform\n"
            "%4 = Toggle display of the IR waveform\n"
            "%5 = Toggle display of the Raman waveform\n\n"
            "You can change these key values in the %1\n"
            "Preferences->Ancillary Programs window\n\n\n"
            "You can turn off this startup message in the\n"
            "waveformMonitor Preferences window.").arg (parentName).arg (QString (ac[0])).arg (QString (ac[1])).arg (QString (ac[2])).arg (QString (ac[3]));

      if (startup_message) QMessageBox::information (this, tr ("waveformMonitor Startup Message"), startupMessageText);

      first = NVFalse;
    }
}



//  Signal from the map class.

void 
waveformMonitor::slotKeyPress (QKeyEvent *e)
{
  char key[20];
  strcpy (key, e->text ().toLatin1 ());

  if (key[0] == (char) ac[0])
    {
      if (display[APD])
        {
          display[APD] = NVFalse;
        }
      else
        {
          display[APD] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (key[0] == (char) ac[1])
    {
      if (display[PMT])
        {
          display[PMT] = NVFalse;
        }
      else
        {
          display[PMT] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (key[0] == (char) ac[2])
    {
      if (display[IR])
        {
          display[IR] = NVFalse;
        }
      else
        {
          display[IR] = NVTrue;
        }

      force_redraw = NVTrue;
    }


  if (key[0] == (char) ac[3])
    {
      if (display[RAMAN])
        {
          display[RAMAN] = NVFalse;
        }
      else
        {
          display[RAMAN] = NVTrue;
        }

      force_redraw = NVTrue;
    }
}



//  A bunch of slots.

void 
waveformMonitor::slotQuit ()
{
  //  Let the parent program know that we have died from something other than the kill switch from the parent.

  if (abe_share->key != kill_switch) abe_share->killed = kill_switch;


  envout ();


  //  Let go of the shared memory.

  abeShare->detach ();


  exit (0);
}



void 
waveformMonitor::slotMode (bool on)
{
  wave_line_mode = on;

  if (on)
    {
      bMode->setIcon (QIcon (":/icons/mode_line.png"));
    }
  else
    {
      bMode->setIcon (QIcon (":/icons/mode_dot.png"));
    }

  force_redraw = NVTrue;
}



void 
waveformMonitor::setFields ()
{
  if (pos_format == "hdms") bGrp->button (0)->setChecked (true);
  if (pos_format == "hdm") bGrp->button (1)->setChecked (true);
  if (pos_format == "hd") bGrp->button (2)->setChecked (true);
  if (pos_format == "dms") bGrp->button (3)->setChecked (true);
  if (pos_format == "dm") bGrp->button (4)->setChecked (true);
  if (pos_format == "d") bGrp->button (5)->setChecked (true);


  if (startup_message)
    {
      sMessage->setCheckState (Qt::Checked);
    }
  else
    {
      sMessage->setCheckState (Qt::Unchecked);
    }
      

  int32_t hue, sat, val;

  for (int32_t i = 0 ; i < 4 ; i++)
    {
      waveColor[i].getHsv (&hue, &sat, &val);
      if (val < 128)
	{
	  bWavePalette[i].setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
	  bWavePalette[i].setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
	}
      else
	{
	  bWavePalette[i].setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
	  bWavePalette[i].setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
	}
      bWavePalette[i].setColor (QPalette::Normal, QPalette::Button, waveColor[i]);
      bWavePalette[i].setColor (QPalette::Inactive, QPalette::Button, waveColor[i]);
      bWaveColor[i]->setPalette (bWavePalette[i]);
      acZeroColor[i] = waveColor[i];
      acZeroColor[i].setAlpha (128);
    }

  surfaceColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bSurfacePalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bSurfacePalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bSurfacePalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bSurfacePalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bSurfacePalette.setColor (QPalette::Normal, QPalette::Button, surfaceColor);
  bSurfacePalette.setColor (QPalette::Inactive, QPalette::Button, surfaceColor);
  bSurfaceColor->setPalette (bSurfacePalette);


  primaryColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bPrimaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bPrimaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bPrimaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bPrimaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bPrimaryPalette.setColor (QPalette::Normal, QPalette::Button, primaryColor);
  bPrimaryPalette.setColor (QPalette::Inactive, QPalette::Button, primaryColor);
  bPrimaryColor->setPalette (bPrimaryPalette);


  secondaryColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bSecondaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bSecondaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bSecondaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bSecondaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bSecondaryPalette.setColor (QPalette::Normal, QPalette::Button, secondaryColor);
  bSecondaryPalette.setColor (QPalette::Inactive, QPalette::Button, secondaryColor);
  bSecondaryColor->setPalette (bSecondaryPalette);


  backgroundColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bBackgroundPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bBackgroundPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bBackgroundPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bBackgroundPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bBackgroundPalette.setColor (QPalette::Normal, QPalette::Button, backgroundColor);
  bBackgroundPalette.setColor (QPalette::Inactive, QPalette::Button, backgroundColor);
  bBackgroundColor->setPalette (bBackgroundPalette);
}



void
waveformMonitor::slotPrefs ()
{
  prefsD = new QDialog (this, (Qt::WindowFlags) Qt::WA_DeleteOnClose);
  prefsD->setWindowTitle (tr ("waveformMonitor Preferences"));
  prefsD->setModal (true);

  QVBoxLayout *vbox = new QVBoxLayout (prefsD);
  vbox->setMargin (5);
  vbox->setSpacing (5);

  QGroupBox *fbox = new QGroupBox (tr ("Position Format"), prefsD);
  fbox->setWhatsThis (bGrpText);

  QRadioButton *hdms = new QRadioButton (tr ("Hemisphere Degrees Minutes Seconds.decimal"));
  QRadioButton *hdm_ = new QRadioButton (tr ("Hemisphere Degrees Minutes.decimal"));
  QRadioButton *hd__ = new QRadioButton (tr ("Hemisphere Degrees.decimal"));
  QRadioButton *sdms = new QRadioButton (tr ("+/-Degrees Minutes Seconds.decimal"));
  QRadioButton *sdm_ = new QRadioButton (tr ("+/-Degrees Minutes.decimal"));
  QRadioButton *sd__ = new QRadioButton (tr ("+/-Degrees.decimal"));

  bGrp = new QButtonGroup (prefsD);
  bGrp->setExclusive (true);
  connect (bGrp, SIGNAL (buttonClicked (int)), this, SLOT (slotPosClicked (int)));

  bGrp->addButton (hdms, 0);
  bGrp->addButton (hdm_, 1);
  bGrp->addButton (hd__, 2);
  bGrp->addButton (sdms, 3);
  bGrp->addButton (sdm_, 4);
  bGrp->addButton (sd__, 5);

  QHBoxLayout *fboxSplit = new QHBoxLayout;
  QVBoxLayout *fboxLeft = new QVBoxLayout;
  QVBoxLayout *fboxRight = new QVBoxLayout;
  fboxSplit->addLayout (fboxLeft);
  fboxSplit->addLayout (fboxRight);
  fboxLeft->addWidget (hdms);
  fboxLeft->addWidget (hdm_);
  fboxLeft->addWidget (hd__);
  fboxRight->addWidget (sdms);
  fboxRight->addWidget (sdm_);
  fboxRight->addWidget (sd__);
  fbox->setLayout (fboxSplit);

  vbox->addWidget (fbox, 1);

  if (pos_format == "hdms") bGrp->button (0)->setChecked (true);
  if (pos_format == "hdm") bGrp->button (1)->setChecked (true);
  if (pos_format == "hd") bGrp->button (2)->setChecked (true);
  if (pos_format == "dms") bGrp->button (3)->setChecked (true);
  if (pos_format == "dm") bGrp->button (4)->setChecked (true);
  if (pos_format == "d") bGrp->button (5)->setChecked (true);


  QGroupBox *cbox = new QGroupBox (tr ("Colors"), this);
  QVBoxLayout *cboxLayout = new QVBoxLayout;
  cbox->setLayout (cboxLayout);
  QHBoxLayout *cboxTopLayout = new QHBoxLayout;
  QHBoxLayout *cboxBottomLayout = new QHBoxLayout;
  cboxLayout->addLayout (cboxTopLayout);
  cboxLayout->addLayout (cboxBottomLayout);


  QButtonGroup *bGrp = new QButtonGroup (this);
  connect (bGrp, SIGNAL (buttonClicked (int)), this, SLOT (slotWaveColor (int)));


  bWaveColor[PMT] = new QPushButton (tr ("Deep"), this);
  bWaveColor[PMT]->setToolTip (tr ("Change deep color"));
  bWaveColor[PMT]->setWhatsThis (bWaveColor[PMT]->toolTip ());
  bWavePalette[PMT] = bWaveColor[PMT]->palette ();
  cboxTopLayout->addWidget (bWaveColor[PMT]);


  bWaveColor[APD] = new QPushButton (tr ("Shallow"), this);
  bWaveColor[APD]->setToolTip (tr ("Change shallow color"));
  bWaveColor[APD]->setWhatsThis (bWaveColor[APD]->toolTip ());
  bWavePalette[APD] = bWaveColor[APD]->palette ();
  cboxTopLayout->addWidget (bWaveColor[APD]);


  bWaveColor[IR] = new QPushButton (tr ("IR"), this);
  bWaveColor[IR]->setToolTip (tr ("Change IR color"));
  bWaveColor[IR]->setWhatsThis (bWaveColor[IR]->toolTip ());
  bWavePalette[IR] = bWaveColor[IR]->palette ();
  cboxTopLayout->addWidget (bWaveColor[IR]);


  bWaveColor[RAMAN] = new QPushButton (tr ("Raman"), this);
  bWaveColor[RAMAN]->setToolTip (tr ("Change Raman color"));
  bWaveColor[RAMAN]->setWhatsThis (bWaveColor[RAMAN]->toolTip ());
  bWavePalette[RAMAN] = bWaveColor[RAMAN]->palette ();
  cboxTopLayout->addWidget (bWaveColor[RAMAN]);


  bSurfaceColor = new QPushButton (tr ("Surface"), this);
  bSurfaceColor->setToolTip (tr ("Change surface return marker color"));
  bSurfaceColor->setWhatsThis (bSurfaceColor->toolTip ());
  bSurfacePalette = bSurfaceColor->palette ();
  connect (bSurfaceColor, SIGNAL (clicked ()), this, SLOT (slotSurfaceColor ()));
  cboxBottomLayout->addWidget (bSurfaceColor);


  bPrimaryColor = new QPushButton (tr ("Primary"), this);
  bPrimaryColor->setToolTip (tr ("Change primary return marker color"));
  bPrimaryColor->setWhatsThis (bPrimaryColor->toolTip ());
  bPrimaryPalette = bPrimaryColor->palette ();
  connect (bPrimaryColor, SIGNAL (clicked ()), this, SLOT (slotPrimaryColor ()));
  cboxBottomLayout->addWidget (bPrimaryColor);


  bSecondaryColor = new QPushButton (tr ("Secondary"), this);
  bSecondaryColor->setToolTip (tr ("Change secondary return marker color"));
  bSecondaryColor->setWhatsThis (bSecondaryColor->toolTip ());
  bSecondaryPalette = bSecondaryColor->palette ();
  connect (bSecondaryColor, SIGNAL (clicked ()), this, SLOT (slotSecondaryColor ()));
  cboxBottomLayout->addWidget (bSecondaryColor);


  bBackgroundColor = new QPushButton (tr ("Background"), this);
  bBackgroundColor->setToolTip (tr ("Change display background color"));
  bBackgroundColor->setWhatsThis (bBackgroundColor->toolTip ());
  bBackgroundPalette = bBackgroundColor->palette ();
  connect (bBackgroundColor, SIGNAL (clicked ()), this, SLOT (slotBackgroundColor ()));
  cboxBottomLayout->addWidget (bBackgroundColor);


  vbox->addWidget (cbox, 1);


  QGroupBox *mBox = new QGroupBox (tr ("Display startup message"), this);
  QHBoxLayout *mBoxLayout = new QHBoxLayout;
  mBox->setLayout (mBoxLayout);
  sMessage = new QCheckBox (mBox);
  sMessage->setToolTip (tr ("Toggle display of startup message when program starts"));
  mBoxLayout->addWidget (sMessage);


  vbox->addWidget (mBox, 1);


  QHBoxLayout *actions = new QHBoxLayout (0);
  vbox->addLayout (actions);

  QPushButton *bHelp = new QPushButton (prefsD);
  bHelp->setIcon (QIcon (":/icons/contextHelp.png"));
  bHelp->setToolTip (tr ("Enter What's This mode for help"));
  connect (bHelp, SIGNAL (clicked ()), this, SLOT (slotHelp ()));
  actions->addWidget (bHelp);

  actions->addStretch (10);

  bRestoreDefaults = new QPushButton (tr ("Restore Defaults"), this);
  bRestoreDefaults->setToolTip (tr ("Restore all preferences to the default state"));
  bRestoreDefaults->setWhatsThis (restoreDefaultsText);
  connect (bRestoreDefaults, SIGNAL (clicked ()), this, SLOT (slotRestoreDefaults ()));
  actions->addWidget (bRestoreDefaults);

  QPushButton *closeButton = new QPushButton (tr ("Close"), prefsD);
  closeButton->setToolTip (tr ("Close the preferences dialog"));
  connect (closeButton, SIGNAL (clicked ()), this, SLOT (slotClosePrefs ()));
  actions->addWidget (closeButton);


  setFields ();


  prefsD->show ();
}



void
waveformMonitor::slotPosClicked (int id)
{
  switch (id)
    {
    case 0:
    default:
      pos_format = "hdms";
      break;

    case 1:
      pos_format = "hdm";
      break;

    case 2:
      pos_format = "hd";
      break;

    case 3:
      pos_format = "dms";
      break;

    case 4:
      pos_format = "dm";
      break;

    case 5:
      pos_format = "d";
      break;
    }
}



void
waveformMonitor::slotClosePrefs ()
{
  if (sMessage->checkState () == Qt::Checked)
    {
      startup_message = NVTrue;
    }
  else
    {
      startup_message = NVFalse;
    }

  prefsD->close ();
}



void
waveformMonitor::slotWaveColor (int id)
{
  QString title;
  switch (id)
    {
    case 0:
      title = tr ("waveformMonitor Deep Color");
      break;

    case 1:
      title = tr ("waveformMonitor Shallow Color");
      break;

    case 2:
      title = tr ("waveformMonitor IR Color");
      break;

    case 3:
      title = tr ("waveformMonitor Raman Color");
      break;
    }

  QColor clr; 
  clr = QColorDialog::getColor (waveColor[id], this, title, QColorDialog::ShowAlphaChannel);
  if (clr.isValid ()) waveColor[id] = clr;

  acZeroColor[id] = waveColor[id];
  acZeroColor[id].setAlpha (128);

  setFields ();

  force_redraw = NVTrue;
}



void
waveformMonitor::slotSurfaceColor ()
{
  QColor clr;

  clr = QColorDialog::getColor (surfaceColor, this, tr ("waveformMonitor Surface Marker Color"), QColorDialog::ShowAlphaChannel);

  if (clr.isValid ()) surfaceColor = clr;

  setFields ();

  force_redraw = NVTrue;
}



void
waveformMonitor::slotPrimaryColor ()
{
  QColor clr;

  clr = QColorDialog::getColor (primaryColor, this, tr ("waveformMonitor Primary Marker Color"), QColorDialog::ShowAlphaChannel);

  if (clr.isValid ()) primaryColor = clr;

  setFields ();

  force_redraw = NVTrue;
}



void
waveformMonitor::slotSecondaryColor ()
{
  QColor clr;

  clr = QColorDialog::getColor (secondaryColor, this, tr ("waveformMonitor Secondary Marker Color"), QColorDialog::ShowAlphaChannel);

  if (clr.isValid ()) secondaryColor = clr;

  setFields ();

  force_redraw = NVTrue;
}



void
waveformMonitor::slotBackgroundColor ()
{
  QColor clr;

  clr = QColorDialog::getColor (backgroundColor, this, tr ("waveformMonitor Background Color"));

  if (clr.isValid ()) backgroundColor = clr;

  setFields ();

  force_redraw = NVTrue;
}



void 
waveformMonitor::scaleWave (int32_t x, int32_t y, int32_t *new_x, int32_t *new_y, int32_t type, NVMAP_DEF l_mapdef)
{
  *new_y = NINT (((float) (x - bounds[PMT].min_x) / (float) bounds[PMT].range_x) * (float) l_mapdef.draw_height);

  switch (type)
    {
    case RAMAN:
      *new_x = NINT (((float) (y - bounds[RAMAN].min_y) / (float) bounds[RAMAN].range_y) * (float) l_mapdef.draw_width);
      break;

    case IR:
      *new_x = NINT (((float) (y - bounds[IR].min_y) / (float) bounds[IR].range_y) * (float) l_mapdef.draw_width);
      break;

    case PMT:
      *new_x = NINT (((float) (y - bounds[PMT].min_y) / (float) bounds[PMT].range_y) * (float) l_mapdef.draw_width);
      break;

    case APD:
      *new_x = NINT (((float) (y - bounds[APD].min_y) / (float) bounds[APD].range_y) * (float) l_mapdef.draw_width);
      break;
    }
}



void 
waveformMonitor::slotPlotWaves (NVMAP_DEF l_mapdef)
{
  static int32_t          prev_type = -1, save_rec;
  static WAVE             save_wave;
  static HYDRO_OUTPUT_T   save_hof;
  static CZMIL_CPF_Data   save_cpf;
  static QString          save_name;
  int32_t                 pix_x[2], pix_y[2];
  QString                 stat;


  if (!wave_read) return;


  //  Because the trackCursor function may be changing the data while we're still plotting it we save it
  //  to this static structure.  lock_track stops trackCursor from updating while we're trying to get an
  //  atomic snapshot of the data for the latest point.

  lock_track = NVTrue;
  save_wave = wave;
  save_rec = recnum;


  switch (save_wave.type)
    {
    case PFM_SHOALS_1K_DATA:
    case PFM_CHARTS_HOF_DATA:
      save_hof = hof_record;
      save_name = db_name;
      break;

    case PFM_CZMIL_DATA:
      save_cpf = cpf_record;
      break;
    }


  lock_track = NVFalse;


  //  Draw the waveforms.

  for (int32_t j = 3 ; j >= 0 ; j--)
    {
      if (display[j]) 
	{
	  //  Mark the ac_zero offset for each waveform (only for HOF files).

          if (save_wave.type == PFM_SHOALS_1K_DATA || save_wave.type == PFM_CHARTS_HOF_DATA)
	    {
	      Qt::PenStyle pen = Qt::DashLine;

	      switch (j)
		{
		case PMT:
		  pen = Qt::DashLine;
		  break;

		case APD:
		  pen = Qt::DashDotLine;
		  break;

		case IR:
		  pen = Qt::DashDotDotLine;
		  break;

		case RAMAN:
		  pen = Qt::DotLine;
		  break;
		}

	      scaleWave (0, bounds[j].ac_zero_offset, &pix_x[0], &pix_y[0], j, l_mapdef);
	      scaleWave (bounds[j].length, bounds[j].ac_zero_offset, &pix_x[1], &pix_y[1], j, l_mapdef);
	      map->drawLine (pix_x[0], pix_y[0] - 15, pix_x[1], pix_y[1] + 15, acZeroColor[j], 2, NVFalse, pen);
	    }

	  scaleWave (1, save_wave.data[j][0], &pix_x[0], &pix_y[0], j, l_mapdef);

	  for (int32_t i = 1 ; i < bounds[j].length ; i++)
	    {
	      scaleWave (i, save_wave.data[j][i], &pix_x[1], &pix_y[1], j, l_mapdef);

	      if (wave_line_mode)
		{
		  map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], waveColor[j], 2, NVFalse, Qt::SolidLine);
		}
	      else
		{
		  map->fillRectangle (pix_x[0], pix_y[0], SPOT_SIZE, SPOT_SIZE, waveColor[j], NVFalse);
		}
	      pix_x[0] = pix_x[1];
	      pix_y[0] = pix_y[1];
	    }
	}
    }


  record.sprintf ("%d", save_rec);


  nameLabel->setText (save_name);
  dateLabel->setText (date_time);
  recordLabel->setText (record);


  switch (save_wave.type)
    {
    case PFM_SHOALS_1K_DATA:
    case PFM_CHARTS_HOF_DATA:
      if (save_hof.sfc_channel_used == IR)
        {
          if (display[IR]) 
            {
              scaleWave (save_hof.sfc_bin_ir, save_wave.data[IR][save_hof.sfc_bin_ir], &pix_x[0], &pix_y[0], IR, l_mapdef);
              drawX (pix_x[0], pix_y[0], 10, 2, surfaceColor);
            }
          sfc_bin = tr ("IR %1").arg (save_hof.sfc_bin_ir);
        }

      if (save_hof.sfc_channel_used == RAMAN && display[RAMAN])
        {
          if (display[RAMAN]) 
            {
              scaleWave (save_hof.sfc_bin_ram, save_wave.data[RAMAN][save_hof.sfc_bin_ram], &pix_x[0], &pix_y[0], RAMAN,
                         l_mapdef);
              drawX (pix_x[0], pix_y[0], 10, 2, surfaceColor);
            }
          sfc_bin = tr ("Raman %1").arg (save_hof.sfc_bin_ram);
        }


      if (secondary)
        {
          if (save_hof.sec_bot_chan == PMT)
            {
              if (display[PMT]) 
                {
                  scaleWave (save_hof.bot_bin_second, save_wave.data[PMT][save_hof.bot_bin_second], &pix_x[0], &pix_y[0], PMT,
                             l_mapdef);
                  drawX (pix_x[0], pix_y[0], 10, 2, secondaryColor);
                }
              sec_bot_bin = tr ("Deep %d").arg (save_hof.bot_bin_second);
            }
          else
            {
              if (display[APD]) 
                {
                  scaleWave (save_hof.bot_bin_second, save_wave.data[APD][save_hof.bot_bin_second], &pix_x[0], &pix_y[0], APD, l_mapdef);
                  drawX (pix_x[0], pix_y[0], 10, 2, secondaryColor);
                }
              sec_bot_bin = tr ("Shallow %d").arg (save_hof.bot_bin_second);
            }
        }
      else
        {
          sec_bot_bin = "N/A";
        }


      if (save_hof.ab_dep_conf == 72)
        {
          if (display[APD]) 
            {
              scaleWave (save_hof.sfc_bin_apd, save_wave.data[APD][save_hof.sfc_bin_apd], &pix_x[0], &pix_y[0], APD, l_mapdef);
              drawX (pix_x[0], pix_y[0], 10, 2, primaryColor);
            }
          bot_bin = tr ("SL %1").arg (save_hof.sfc_bin_apd);
          sfc_bin = tr ("Shallow %1").arg (save_hof.sfc_bin_apd);
        }
      else if (save_hof.ab_dep_conf == 74)
        {
          //  No marker for shallow water algorithm.

          bot_bin = tr ("SWA %1").arg (save_hof.sfc_bin_apd);
        }
      else if (save_hof.bot_channel == PMT)
        {
          if (display[PMT]) 
            {
              scaleWave (save_hof.bot_bin_first, save_wave.data[PMT][save_hof.bot_bin_first], &pix_x[0], &pix_y[0], PMT, l_mapdef);
              drawX (pix_x[0], pix_y[0], 10, 2, primaryColor);
            }
          bot_bin = tr ("Deep %1").arg (save_hof.bot_bin_first);
        }
      else
        {
          if (display[APD]) 
            {
              scaleWave (save_hof.bot_bin_first, save_wave.data[APD][save_hof.bot_bin_first], &pix_x[0], &pix_y[0], APD, l_mapdef);
              drawX (pix_x[0], pix_y[0], 10, 2, primaryColor);
            }
          bot_bin = tr ("Shallow %1").arg (save_hof.bot_bin_first);
        }


      if (prev_type != save_hof.data_type)
        {
          tideLabel->setToolTip ("");
          correctDepth->setToolTip ("");
          secDepth->setToolTip ("");
        }

      if (save_hof.data_type)
        {
          mode = "KGPS";
          first = QString ("%L1").arg (-save_hof.correct_depth, 0, 'f', 2);
          rep_depth = QString ("%L1").arg (save_hof.kgps_res_elev, 0, 'f', 2);
          level = QString ("%L1").arg (save_hof.kgps_datum, 0, 'f', 3);
          if (prev_type != save_hof.data_type)
            {
              tideLabel->setToolTip (tr ("Datum value"));
              correctDepth->setToolTip (tr ("Datum corrected depth"));
              secDepth->setToolTip (tr ("Datum corrected second depth"));
            }
          abdc = save_hof.kgps_abd_conf;
        }
      else
        {
          mode = "DGPS";
          first = QString ("%L1").arg (-save_hof.correct_depth, 0, 'f', 2);
          rep_depth = QString ("%L1").arg (save_hof.reported_depth, 0, 'f', 2);
          level = QString ("%L1").arg (-tide, 0, 'f', 3);
          if (prev_type != save_hof.data_type)
            {
              tideLabel->setToolTip (tr ("Tide value"));
              correctDepth->setToolTip (tr ("Tide corrected depth"));
              secDepth->setToolTip (tr ("Tide corrected second depth"));
            }
          abdc = save_hof.abdc;
        }
      prev_type = save_hof.data_type;

      if (secondary)
        {
          sec_depth = QString ("%L1").arg (-save_hof.correct_sec_depth, 0, 'f', 2);
          slat = QString ("%L1").arg (save_hof.sec_latitude, 0, 'f', 9);
          slon = QString ("%L1").arg (save_hof.sec_longitude, 0, 'f', 9);
          if (save_hof.data_type)
            {
              sabdc = QString ("%1").arg (save_hof.kgps_sec_abd_conf);
            }
          else
            {
              sabdc = QString ("%1").arg (save_hof.sec_abdc);
            }
          ssig = QString ("%L1").arg (save_hof.sec_bot_conf, 0, 'f', 1);
          s_full_conf = QString ("%1").arg (save_hof.sec_depth_conf);
        }
      else
        {
          sec_depth = tr ("N/A", "Not Applicable");
          sabdc = tr ("N/A", "Not Applicable");
          ssig = tr ("N/A", "Not Applicable");
          s_full_conf = tr ("N/A", "Not Applicable");
        }

      wave_height = QString ("%L1").arg (save_hof.wave_height, 0, 'f', 2);

      f_full_conf = QString ("%1").arg (save_hof.depth_conf);

      calc_bfom = QString ("%1").arg (save_hof.calc_bfom_thresh_times10[0]);
      sec_calc_bfom = QString ("%1").arg (save_hof.calc_bfom_thresh_times10[1]);


      fsig = QString ("%L1").arg (save_hof.bot_conf, 0, 'f', 1);


      //  Set the status bar labels

      correctDepth->setText (first);
      reportedDepth->setText (rep_depth);
      tideLabel->setText (level);
      dataType->setText (mode);
      abdcLabel->setNum (abdc);
      secFullConf->setText (s_full_conf);
      secDepth->setText (sec_depth);
      sabdcLabel->setText (sabdc);
      waveHeight->setText (wave_height);
      fullConf->setText (f_full_conf);
      bfomThresh->setText (calc_bfom);
      secBfomThresh->setText (sec_calc_bfom);
      botBinLabel->setText (bot_bin);
      secBotBinLabel->setText (sec_bot_bin);
      sfcBinLabel->setText (sfc_bin);
      sigStrength->setText (fsig);
      secSigStrength->setText (ssig);


      if (save_hof.status & AU_STATUS_DELETED_BIT) 
        {
          stat = "Deleted";
        }
      else if (save_hof.status & AU_STATUS_KEPT_BIT)
        {
          stat = "Kept";
        }
      else if (save_hof.status & AU_STATUS_SWAPPED_BIT) 
        {
          stat = "Swapped";
        }
      else
        {
          stat = " ";
        }
      break;


    case PFM_CZMIL_DATA:


      if (display[PMT]) 
        {
          if (save_cpf.channel[CZMIL_DEEP_CHANNEL][save_wave.ret].interest_point != 0.0)
            {
              int32_t prev_point = (int32_t) save_cpf.channel[CZMIL_DEEP_CHANNEL][save_wave.ret].interest_point;
              int32_t next_point = (int32_t) save_cpf.channel[CZMIL_DEEP_CHANNEL][save_wave.ret].interest_point + 1;

              int32_t cy = save_wave.data[PMT][prev_point] + ((save_wave.data[PMT][next_point] - save_wave.data[PMT][prev_point]) / 2);

              int32_t cx = save_cpf.channel[CZMIL_DEEP_CHANNEL][save_wave.ret].interest_point;

              scaleWave (cx, cy, &pix_x[0], &pix_y[0], PMT, l_mapdef);

              drawX (pix_x[0], pix_y[0], 10, 2, primaryColor);
            }
        }


      if (display[APD])
        {
          if (save_cpf.channel[save_wave.chan][save_wave.ret].interest_point != 0.0)
            {
              int32_t prev_point = (int32_t) save_cpf.channel[save_wave.chan][save_wave.ret].interest_point;
              int32_t next_point = (int32_t) save_cpf.channel[save_wave.chan][save_wave.ret].interest_point + 1;

              int32_t cy = save_wave.data[APD][prev_point] + ((save_wave.data[APD][next_point] - save_wave.data[APD][prev_point]) / 2);

              int32_t cx = save_cpf.channel[save_wave.chan][save_wave.ret].interest_point;

              scaleWave (cx, cy, &pix_x[0], &pix_y[0], APD, l_mapdef);

              drawX (pix_x[0], pix_y[0], 10, 2, primaryColor);
            }
        }

      botBinLabel->setText (bot_bin);
      secBotBinLabel->setText (sec_bot_bin);

      break;
    }


  int32_t w = l_mapdef.draw_width, h = l_mapdef.draw_height;

  map->drawText ("Shallow", w - 90, h - 90, waveColor[APD], NVTrue);
  map->drawText ("Deep", w - 90, h - 70, waveColor[PMT], NVTrue);
  map->drawText ("IR", w - 90, h - 50, waveColor[IR], NVTrue);
  if (save_wave.type != PFM_CZMIL_DATA) map->drawText ("Raman", w - 90, h - 30, waveColor[RAMAN], NVTrue);
  map->drawText (stat, w - 90, h - 10, surfaceColor, NVTrue);
}



void 
waveformMonitor::drawX (int32_t x, int32_t y, int32_t size, int32_t width, QColor color)
{
  int32_t hs = size / 2;

  map->drawLine (x - hs, y + hs, x + hs, y - hs, color, width, NVTrue, Qt::SolidLine);
  map->drawLine (x + hs, y + hs, x - hs, y - hs, color, width, NVTrue, Qt::SolidLine);
}



void
waveformMonitor::slotRestoreDefaults ()
{
  static uint8_t first = NVTrue;

  pos_format = "hdms";
  width = WAVE_X_SIZE;
  height = WAVE_Y_SIZE;
  window_x = 0;
  window_y = 0;
  waveColor[APD] = Qt::yellow;
  waveColor[PMT] = Qt::white;
  waveColor[IR] = Qt::red;
  waveColor[RAMAN] = Qt::cyan;
  surfaceColor = Qt::yellow;
  primaryColor = Qt::green;
  secondaryColor = Qt::red;
  backgroundColor = Qt::black;
  display[APD] = NVTrue;
  display[PMT] = NVTrue;
  display[IR] = NVTrue;
  display[RAMAN] = NVTrue;
  startup_message = NVTrue;


  //  The first time will be called from envin and the prefs dialog won't exist yet.

  if (!first) setFields ();
  first = NVFalse;

  force_redraw = NVTrue;
}



void
waveformMonitor::about ()
{
  QMessageBox::about (this, VERSION,
                      "waveformMonitor - CHARTS wave form monitor."
                      "\n\nAuthor : Jan C. Depner (area.based.editor@gmail.com)");
}


void
waveformMonitor::slotAcknowledgments ()
{
  QMessageBox::about (this, VERSION, acknowledgmentsText);
}



void
waveformMonitor::aboutQt ()
{
  QMessageBox::aboutQt (this, VERSION);
}



//  Get the users defaults.

void
waveformMonitor::envin ()
{
  //  We need to get the font from the global settings.

#ifdef NVWIN3X
  QString ini_file2 = QString (getenv ("USERPROFILE")) + "/ABE.config/" + "globalABE.ini";
#else
  QString ini_file2 = QString (getenv ("HOME")) + "/ABE.config/" + "globalABE.ini";
#endif

  font = QApplication::font ();

  QSettings settings2 (ini_file2, QSettings::IniFormat);
  settings2.beginGroup ("globalABE");


  QString defaultFont = font.toString ();
  QString fontString = settings2.value (QString ("ABE map GUI font"), defaultFont).toString ();
  font.fromString (fontString);


  settings2.endGroup ();


  double saved_version = 0.0;


  // Set Defaults so the if keys don't exist the parms are defined

  slotRestoreDefaults ();
  force_redraw = NVFalse;


  //  Get the INI file name

#ifdef NVWIN3X
  QString ini_file = QString (getenv ("USERPROFILE")) + "/ABE.config/waveformMonitor.ini";
#else
  QString ini_file = QString (getenv ("HOME")) + "/ABE.config/waveformMonitor.ini";
#endif

  QSettings settings (ini_file, QSettings::IniFormat);
  settings.beginGroup ("waveformMonitor");

  saved_version = settings.value (QString ("settings version"), saved_version).toDouble ();


  //  If the settings version has changed we need to leave the values at the new defaults since they may have changed.

  if (settings_version != saved_version) return;


  pos_format = settings.value (QString ("position form"), pos_format).toString ();

  width = settings.value (QString ("width"), width).toInt ();

  height = settings.value (QString ("height"), height).toInt ();

  window_x = settings.value (QString ("window x"), window_x).toInt ();

  window_y = settings.value (QString ("window y"), window_y).toInt ();


  startup_message = settings.value (QString ("Display Startup Message"), startup_message).toBool ();


  display[APD] = settings.value (QString ("Display Shallow"), display[APD]).toBool ();
  display[PMT] = settings.value (QString ("Display Deep"), display[PMT]).toBool ();
  display[IR] = settings.value (QString ("Display IR"), display[IR]).toBool ();
  display[RAMAN] = settings.value (QString ("Display Raman"), display[RAMAN]).toBool ();

  wave_line_mode = settings.value (QString ("Wave line mode flag"), wave_line_mode).toBool ();


  int32_t red = settings.value (QString ("Shallow color/red"), waveColor[APD].red ()).toInt ();
  int32_t green = settings.value (QString ("Shallow color/green"), waveColor[APD].green ()).toInt ();
  int32_t blue = settings.value (QString ("Shallow color/blue"), waveColor[APD].blue ()).toInt ();
  int32_t alpha = settings.value (QString ("Shallow color/alpha"), waveColor[APD].alpha ()).toInt ();
  waveColor[APD].setRgb (red, green, blue, alpha);

  red = settings.value (QString ("Deep color/red"), waveColor[PMT].red ()).toInt ();
  green = settings.value (QString ("Deep color/green"), waveColor[PMT].green ()).toInt ();
  blue = settings.value (QString ("Deep color/blue"), waveColor[PMT].blue ()).toInt ();
  alpha = settings.value (QString ("Deep color/alpha"), waveColor[PMT].alpha ()).toInt ();
  waveColor[PMT].setRgb (red, green, blue, alpha);

  red = settings.value (QString ("IR color/red"), waveColor[IR].red ()).toInt ();
  green = settings.value (QString ("IR color/green"), waveColor[IR].green ()).toInt ();
  blue = settings.value (QString ("IR color/blue"), waveColor[IR].blue ()).toInt ();
  alpha = settings.value (QString ("IR color/alpha"), waveColor[IR].alpha ()).toInt ();
  waveColor[IR].setRgb (red, green, blue, alpha);

  red = settings.value (QString ("Raman color/red"), waveColor[RAMAN].red ()).toInt ();
  green = settings.value (QString ("Raman color/green"), waveColor[RAMAN].green ()).toInt (); 
  blue = settings.value (QString ("Raman color/blue"), waveColor[RAMAN].blue ()).toInt ();
  alpha = settings.value (QString ("Raman color/alpha"), waveColor[RAMAN].alpha ()).toInt ();
  waveColor[RAMAN].setRgb (red, green, blue, alpha);

  red = settings.value (QString ("Surface color/red"), surfaceColor.red ()).toInt ();
  green = settings.value (QString ("Surface color/green"), surfaceColor.green ()).toInt ();
  blue = settings.value (QString ("Surface color/blue"), surfaceColor.blue ()).toInt ();
  alpha = settings.value (QString ("Surface color/alpha"), surfaceColor.alpha ()).toInt ();
  surfaceColor.setRgb (red, green, blue, alpha);

  red = settings.value (QString ("First color/red"), primaryColor.red ()).toInt ();
  green = settings.value (QString ("First color/green"), primaryColor.green ()).toInt ();
  blue = settings.value (QString ("First color/blue"), primaryColor.blue ()).toInt ();
  alpha = settings.value (QString ("First color/alpha"), primaryColor.alpha ()).toInt ();
  primaryColor.setRgb (red, green, blue, alpha);

  red = settings.value (QString ("Second color/red"), secondaryColor.red ()).toInt ();
  green = settings.value (QString ("Second color/green"), secondaryColor.green ()).toInt ();
  blue = settings.value (QString ("Second color/blue"), secondaryColor.blue ()).toInt ();
  alpha = settings.value (QString ("Second color/alpha"), secondaryColor.alpha ()).toInt ();
  secondaryColor.setRgb (red, green, blue, alpha);

  red = settings.value (QString ("Background color/red"), backgroundColor.red ()).toInt ();
  green = settings.value (QString ("Background color/green"), backgroundColor.green ()).toInt ();
  blue = settings.value (QString ("Background color/blue"), backgroundColor.blue ()).toInt ();
  alpha = settings.value (QString ("Background color/alpha"), backgroundColor.alpha ()).toInt ();
  backgroundColor.setRgb (red, green, blue, alpha);


  //  Set the ac zero line colors.

  for (int32_t i = 0 ; i < 4 ; i++)
    {
      acZeroColor[i] = waveColor[i];
      acZeroColor[i].setAlpha (128);
    }

  this->restoreState (settings.value (QString ("main window state")).toByteArray (), NINT (settings_version * 100.0));

  settings.endGroup ();
}




//  Save the users defaults.

void
waveformMonitor::envout ()
{
  //  Use frame geometry to get the absolute x and y.

  QRect tmp = this->frameGeometry ();

  window_x = tmp.x ();
  window_y = tmp.y ();


  //  Use geometry to get the width and height.

  tmp = this->geometry ();
  width = tmp.width ();
  height = tmp.height ();


  //  Get the INI file name

#ifdef NVWIN3X
  QString ini_file = QString (getenv ("USERPROFILE")) + "/ABE.config/waveformMonitor.ini";
#else
  QString ini_file = QString (getenv ("HOME")) + "/ABE.config/waveformMonitor.ini";
#endif

  QSettings settings (ini_file, QSettings::IniFormat);
  settings.beginGroup ("waveformMonitor");


  settings.setValue (QString ("settings version"), settings_version);

  settings.setValue (QString ("position form"), pos_format);

  settings.setValue (QString ("width"), width);

  settings.setValue (QString ("height"), height);

  settings.setValue (QString ("window x"), window_x);

  settings.setValue (QString ("window y"), window_y);


  settings.setValue (QString ("Display Startup Message"), startup_message);


  settings.setValue (QString ("Display Shallow"), display[APD]);
  settings.setValue (QString ("Display Deep"), display[PMT]);
  settings.setValue (QString ("Display IR"), display[IR]);
  settings.setValue (QString ("Display Raman"), display[RAMAN]);

  settings.setValue (QString ("Wave line mode flag"), wave_line_mode);


  settings.setValue (QString ("Deep color/red"), waveColor[PMT].red ());
  settings.setValue (QString ("Deep color/green"), waveColor[PMT].green ());
  settings.setValue (QString ("Deep color/blue"), waveColor[PMT].blue ());
  settings.setValue (QString ("Deep color/alpha"), waveColor[PMT].alpha ());

  settings.setValue (QString ("Shallow color/red"), waveColor[APD].red ());
  settings.setValue (QString ("Shallow color/green"), waveColor[APD].green ());
  settings.setValue (QString ("Shallow color/blue"), waveColor[APD].blue ());
  settings.setValue (QString ("Shallow color/alpha"), waveColor[APD].alpha ());

  settings.setValue (QString ("IR color/red"), waveColor[IR].red ());
  settings.setValue (QString ("IR color/green"), waveColor[IR].green ());
  settings.setValue (QString ("IR color/blue"), waveColor[IR].blue ());
  settings.setValue (QString ("IR color/alpha"), waveColor[IR].alpha ());

  settings.setValue (QString ("Raman color/red"), waveColor[RAMAN].red ());
  settings.setValue (QString ("Raman color/green"), waveColor[RAMAN].green ());
  settings.setValue (QString ("Raman color/blue"), waveColor[RAMAN].blue ());
  settings.setValue (QString ("Raman color/alpha"), waveColor[RAMAN].alpha ());

  settings.setValue (QString ("Surface color/red"), surfaceColor.red ());
  settings.setValue (QString ("Surface color/green"), surfaceColor.green ());
  settings.setValue (QString ("Surface color/blue"), surfaceColor.blue ());
  settings.setValue (QString ("Surface color/alpha"), surfaceColor.alpha ());

  settings.setValue (QString ("First color/red"), primaryColor.red ());
  settings.setValue (QString ("First color/green"), primaryColor.green ());
  settings.setValue (QString ("First color/blue"), primaryColor.blue ());
  settings.setValue (QString ("First color/alpha"), primaryColor.alpha ());

  settings.setValue (QString ("Second color/red"), secondaryColor.red ());
  settings.setValue (QString ("Second color/green"), secondaryColor.green ());
  settings.setValue (QString ("Second color/blue"), secondaryColor.blue ());
  settings.setValue (QString ("Second color/alpha"), secondaryColor.alpha ());

  settings.setValue (QString ("Background color/red"), backgroundColor.red ());
  settings.setValue (QString ("Background color/green"), backgroundColor.green ());
  settings.setValue (QString ("Background color/blue"), backgroundColor.blue ());
  settings.setValue (QString ("Background color/alpha"), backgroundColor.alpha ());


  settings.setValue (QString ("main window state"), this->saveState (NINT (settings_version * 100.0)));

  settings.endGroup ();
}
