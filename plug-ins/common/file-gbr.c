/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * gbr plug-in version 1.00
 * Loads/exports version 2 GIMP .gbr files, by Tim Newsome <drz@frody.bloke.com>
 * Some bits stolen from the .99.7 source tree.
 *
 * Added in GBR version 1 support after learning that there wasn't a
 * tool to read them.
 * July 6, 1998 by Seth Burgess <sjburges@gimp.org>
 *
 * Dec 17, 2000
 * Load and save GIMP brushes in GRAY or RGBA.  jtl + neo
 *
 *
 * TODO: Give some better error reporting on not opening files/bad headers
 *       etc.
 */

#include "config.h"

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "app/core/gimpbrush-header.h"
#include "app/core/gimppattern-header.h"

#include "libgimp/stdplugins-intl.h"


#define SAVE_PROC      "file-gbr-save"
#define PLUG_IN_BINARY "file-gbr"
#define PLUG_IN_ROLE   "gimp-file-gbr"


typedef struct
{
  gchar description[256];
  gint  spacing;
} BrushInfo;


/*  local function prototypes  */

static void       query          (void);
static void       run            (const gchar      *name,
                                  gint              nparams,
                                  const GimpParam  *param,
                                  gint             *nreturn_vals,
                                  GimpParam       **return_vals);

static gboolean   save_image     (GFile            *file,
                                  gint32            image_ID,
                                  gint32            drawable_ID,
                                  GError          **error);

static gboolean   save_dialog    (void);
static void       entry_callback (GtkWidget        *widget,
                                  gpointer          data);


const GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};


/*  private variables  */

static BrushInfo info =
{
  "GIMP Brush",
  10
};


MAIN ()

static void
query (void)
{
  static const GimpParamDef save_args[] =
  {
    { GIMP_PDB_INT32,    "run-mode",    "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
    { GIMP_PDB_IMAGE,    "image",       "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",    "Drawable to export" },
    { GIMP_PDB_STRING,   "uri",         "The URI of the file to export the image in" },
    { GIMP_PDB_STRING,   "raw-uri",     "The URI of the file to export the image in" },
    { GIMP_PDB_INT32,    "spacing",     "Spacing of the brush" },
    { GIMP_PDB_STRING,   "description", "Short description of the brush" }
  };

  gimp_install_procedure (SAVE_PROC,
                          "Exports files in the GIMP brush file format",
                          "Exports files in the GIMP brush file format",
                          "Tim Newsome, Jens Lautenbacher, Sven Neumann",
                          "Tim Newsome, Jens Lautenbacher, Sven Neumann",
                          "1997-2000",
                          N_("GIMP brush"),
                          "RGB*, GRAY*, INDEXED*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);

  gimp_plugin_icon_register (SAVE_PROC, GIMP_ICON_TYPE_ICON_NAME,
                             (const guint8 *) GIMP_ICON_BRUSH);
  gimp_register_file_handler_mime (SAVE_PROC, "image/x-gimp-gbr");
  gimp_register_file_handler_uri (SAVE_PROC);
  gimp_register_save_handler (SAVE_PROC, "gbr", "");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[2];
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  gint32             image_ID;
  gint32             drawable_ID;
  GimpExportReturn   export = GIMP_EXPORT_CANCEL;
  GError            *error  = NULL;

  INIT_I18N ();
  gegl_init (NULL, NULL);

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  if (strcmp (name, SAVE_PROC) == 0)
    {
      GFile        *file;
      GimpParasite *parasite;
      gint32        orig_image_ID;

      image_ID    = param[1].data.d_int32;
      drawable_ID = param[2].data.d_int32;
      file        = g_file_new_for_uri (param[3].data.d_string);

      orig_image_ID = image_ID;

      switch (run_mode)
        {
        case GIMP_RUN_INTERACTIVE:
        case GIMP_RUN_WITH_LAST_VALS:
          gimp_ui_init (PLUG_IN_BINARY, FALSE);

          export = gimp_export_image (&image_ID, &drawable_ID, "GBR",
                                      GIMP_EXPORT_CAN_HANDLE_GRAY    |
                                      GIMP_EXPORT_CAN_HANDLE_RGB     |
                                      GIMP_EXPORT_CAN_HANDLE_INDEXED |
                                      GIMP_EXPORT_CAN_HANDLE_ALPHA);

          if (export == GIMP_EXPORT_CANCEL)
            {
              values[0].data.d_status = GIMP_PDB_CANCEL;
              return;
            }

          /*  Possibly retrieve data  */
          gimp_get_data (SAVE_PROC, &info);

          parasite = gimp_image_get_parasite (orig_image_ID,
                                              "gimp-brush-name");
          if (parasite)
            {
              strncpy (info.description,
                       gimp_parasite_data (parasite),
                       MIN (sizeof (info.description),
                            gimp_parasite_data_size (parasite)));
              info.description[sizeof (info.description) - 1] = '\0';

              gimp_parasite_free (parasite);
            }
          else
            {
              gchar *name = g_path_get_basename (gimp_file_get_utf8_name (file));

              if (g_str_has_suffix (name, ".gbr"))
                name[strlen (name) - 4] = '\0';

              if (strlen (name))
                {
                  strncpy (info.description, name, sizeof (info.description));
                  info.description[sizeof (info.description) - 1] = '\0';
                }

              g_free (name);
            }
          break;

        default:
          break;
        }

      switch (run_mode)
        {
        case GIMP_RUN_INTERACTIVE:
          if (! save_dialog ())
            status = GIMP_PDB_CANCEL;
          break;

        case GIMP_RUN_NONINTERACTIVE:
          if (nparams != 7)
            {
              status = GIMP_PDB_CALLING_ERROR;
            }
          else
            {
              info.spacing = (param[5].data.d_int32);
              strncpy (info.description, param[6].data.d_string,
                       sizeof (info.description));
              info.description[sizeof (info.description) - 1] = '\0';
            }
          break;

        default:
          break;
        }

      if (status == GIMP_PDB_SUCCESS)
        {
          if (save_image (file, image_ID, drawable_ID, &error))
            {
              gimp_set_data (SAVE_PROC, &info, sizeof (info));
            }
          else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }
        }

      if (export == GIMP_EXPORT_EXPORT)
        gimp_image_delete (image_ID);

      if (strlen (info.description))
        {
          GimpParasite *parasite;

          parasite = gimp_parasite_new ("gimp-brush-name",
                                        GIMP_PARASITE_PERSISTENT,
                                        strlen (info.description) + 1,
                                        info.description);
          gimp_image_attach_parasite (orig_image_ID, parasite);
          gimp_parasite_free (parasite);
        }
      else
        {
          gimp_image_detach_parasite (orig_image_ID, "gimp-brush-name");
        }
    }
  else
    {
      status = GIMP_PDB_CALLING_ERROR;
    }

  if (status != GIMP_PDB_SUCCESS && error)
    {
      *nreturn_vals = 2;
      values[1].type          = GIMP_PDB_STRING;
      values[1].data.d_string = error->message;
    }

  values[0].data.d_status = status;
}

static gboolean
save_image (GFile   *file,
            gint32   image_ID,
            gint32   drawable_ID,
            GError **error)
{
  GOutputStream   *output;
  GimpBrushHeader  bh;
  guchar          *brush_buf;
  GeglBuffer      *buffer;
  const Babl      *format;
  gint             line;
  gint             x;
  gint             bpp;
  gint             file_bpp;
  gint             width;
  gint             height;
  GimpRGB          gray, white;

  gimp_rgba_set_uchar (&white, 255, 255, 255, 255);

  switch (gimp_drawable_type (drawable_ID))
    {
    case GIMP_GRAY_IMAGE:
      file_bpp = 1;
      format = babl_format ("Y' u8");
      break;

    case GIMP_GRAYA_IMAGE:
      file_bpp = 1;
      format = babl_format ("Y'A u8");
      break;

    default:
      file_bpp = 4;
      format = babl_format ("R'G'B'A u8");
      break;
    }

  bpp = babl_format_get_bytes_per_pixel (format);

  gimp_progress_init_printf (_("Exporting '%s'"),
                             g_file_get_parse_name (file));

  output = G_OUTPUT_STREAM (g_file_replace (file,
                                            NULL, FALSE, G_FILE_CREATE_NONE,
                                            NULL, error));
  if (! output)
    return FALSE;

  buffer = gimp_drawable_get_buffer (drawable_ID);

  width  = gimp_drawable_width  (drawable_ID);
  height = gimp_drawable_height (drawable_ID);

  bh.header_size  = g_htonl (sizeof (GimpBrushHeader) +
                             strlen (info.description) + 1);
  bh.version      = g_htonl (2);
  bh.width        = g_htonl (width);
  bh.height       = g_htonl (height);
  bh.bytes        = g_htonl (file_bpp);
  bh.magic_number = g_htonl (GIMP_BRUSH_MAGIC);
  bh.spacing      = g_htonl (info.spacing);

  if (! g_output_stream_write_all (output, &bh, sizeof (GimpBrushHeader),
                                   NULL, NULL, error))
    {
      GCancellable *cancellable = g_cancellable_new ();

      g_cancellable_cancel (cancellable);
      g_output_stream_close (output, cancellable, NULL);
      g_object_unref (cancellable);

      g_object_unref (output);
      return FALSE;
    }

  if (! g_output_stream_write_all (output,
                                   info.description,
                                   strlen (info.description) + 1,
                                   NULL, NULL, error))
    {
      GCancellable *cancellable = g_cancellable_new ();

      g_cancellable_cancel (cancellable);
      g_output_stream_close (output, cancellable, NULL);
      g_object_unref (cancellable);

      g_object_unref (output);
      return FALSE;
    }

  brush_buf = g_new (guchar, width * bpp);

  for (line = 0; line < height; line++)
    {
      gegl_buffer_get (buffer, GEGL_RECTANGLE (0, line, width, 1), 1.0,
                       format, brush_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

      switch (bpp)
        {
        case 1:
          /*  invert  */
          for (x = 0; x < width; x++)
            brush_buf[x] = 255 - brush_buf[x];
          break;

        case 2:
          for (x = 0; x < width; x++)
            {
              /*  apply alpha channel  */
              gimp_rgba_set_uchar (&gray,
                                   brush_buf[2 * x],
                                   brush_buf[2 * x],
                                   brush_buf[2 * x],
                                   brush_buf[2 * x + 1]);
              gimp_rgb_composite (&gray, &white, GIMP_RGB_COMPOSITE_BEHIND);
              gimp_rgba_get_uchar (&gray, &brush_buf[x], NULL, NULL, NULL);
              /* invert */
              brush_buf[x] = 255 - brush_buf[x];
            }
          break;
        }

      if (! g_output_stream_write_all (output, brush_buf, width * file_bpp,
                                       NULL, NULL, error))
        {
          GCancellable *cancellable = g_cancellable_new ();

          g_cancellable_cancel (cancellable);
          g_output_stream_close (output, cancellable, NULL);
          g_object_unref (cancellable);

          g_free (brush_buf);
          g_object_unref (output);
          return FALSE;
        }

      gimp_progress_update ((gdouble) line / (gdouble) height);
    }

  g_free (brush_buf);
  g_object_unref (buffer);
  g_object_unref (output);

  gimp_progress_update (1.0);

  return TRUE;
}

static gboolean
save_dialog (void)
{
  GtkWidget     *dialog;
  GtkWidget     *grid;
  GtkWidget     *entry;
  GtkWidget     *spinbutton;
  GtkAdjustment *adj;
  gboolean       run;

  dialog = gimp_export_dialog_new (_("Brush"), PLUG_IN_BINARY, SAVE_PROC);

  /* The main grid */
  grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_box_pack_start (GTK_BOX (gimp_export_dialog_get_content_area (dialog)),
                      grid, TRUE, TRUE, 0);
  gtk_widget_show (grid);

  entry = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 20);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_entry_set_text (GTK_ENTRY (entry), info.description);
  gimp_grid_attach_aligned (GTK_GRID (grid), 0, 0,
                            _("Description:"), 1.0, 0.5,
                            entry, 1);

  g_signal_connect (entry, "changed",
                    G_CALLBACK (entry_callback),
                    info.description);

  adj = gtk_adjustment_new (info.spacing, 1, 1000, 1, 10, 0);
  spinbutton = gtk_spin_button_new (adj, 1.0, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (spinbutton), TRUE);
  gimp_grid_attach_aligned (GTK_GRID (grid), 0, 1,
                            _("Spacing:"), 1.0, 0.5,
                            spinbutton, 1);

  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &info.spacing);

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
}

static void
entry_callback (GtkWidget *widget,
                gpointer   data)
{
  strncpy (info.description, gtk_entry_get_text (GTK_ENTRY (widget)),
           sizeof (info.description));
  info.description[sizeof (info.description) - 1] = '\0';
}
