/****************************************************************************
** This requires at least a COMPACT memory model to compile and run.       **
*****************************************************************************
** Demonstration the use of the MIDIPlay unit for playing MIDI files in    **
** the background.                                                         **
**  by Steven H Don                                                        **
**                                                                         **
** For questions, feel free to e-mail me.                                  **
**                                                                         **
**    shd@earthling.net                                                    **
**    http://shd.cjb.net                                                   **
**                                                                         **
****************************************************************************/
#include <io.h>
#include <conio.h>
#include <string.h>
#include "MIDIplay.c"

void main ()
{
  char Name [63];

  //Initialize the MIDI player
  InitMIDI ();
  SetFM (); //or use SetGM here if the PC supports it

  //Clear the screen
  clrscr ();

  //Display the message
  printf ("Playing a MIDI file - by Steven Don\n");
  printf ("-----------------------------------\n\n");
  printf ("Two MIDI files should be included: 1.MID - Ghostbusters theme\n");
  printf ("                                   2.MID - Superman movie theme\n\n");

  //Get the name of a MIDI file
  printf ("Please enter name of file: ");
  gets (Name);

  //Append extension, if necessary
  if (Name [strlen(Name) - 4] != '.') strcat (Name, ".MID");

  //Load it
  printf ("Loading MIDI file - ");
  if (LoadMIDI (Name) != 0)
    printf ("Success\n");
  else {
    printf ("Failed\n");
    return;
  }

  //Play it
  PlayMIDI ();
  printf ("Playing...\n");

  //Wait for a keypress or the end of the song
  do; while (!kbhit() && Playing () == 1);

  //If there has been a key pressed, clear the buffer
  if (kbhit()) getch();

  //If the song had reached the end, display a message
  if (Playing () == 0) printf ("Stopped\n");
  //Halt playback before exiting
  UnloadMIDI ();
}