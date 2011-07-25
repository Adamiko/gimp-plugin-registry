/**********************************************************************
  icc_colorspace.c
  Copyright(C) 2007-2010 Y.Yamakawa
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "icc_colorspace.h"
#include "srgb_profile.h"

#include "platform.h"

#include "iccbutton.h"

#define PARASITE_FLAGS (GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE)

/* Declare local functions. */
static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static void     icc_colorspace_init                    (IccColorspaceContext *context);
static void     icc_colorspace_cleanup                 (IccColorspaceContext *context);
static void     icc_colorspace_store_settings          (IccColorspaceContext *context);

static gboolean icc_colorspace_compare_profile         (const gpointer        ptr1,
                                                        gsize                 size1,
                                                        const gpointer        ptr2,
                                                        gsize                 size2);
static gboolean icc_colorspace_compare_display_profile (cmsHPROFILE           hProfile1,
                                                        cmsHPROFILE           hProfile2);

static gboolean icc_colorspace_assign                  (IccColorspaceContext *context);
static gboolean icc_colorspace_convert                 (IccColorspaceContext *context);
static gboolean icc_colorspace_convert_colormap        (IccColorspaceContext *context);
static gboolean icc_colorspace_convert_layer           (IccColorspaceContext *context,
                                                        GimpDrawable         *drawable,
                                                        gboolean              selection);

static void     icc_button_changed                     (IccButton            *button,
                                                        GtkRadioButton       *radioButton);

static gboolean assign_dialog                          (IccColorspaceContext *context);
static gboolean convert_dialog                         (IccColorspaceContext *context);
static gboolean abstract_dialog                        (IccColorspaceContext *context);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

/* Arguments */
static const GimpParamDef assign_args[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "(unused)" },
  { GIMP_PDB_INT32, "unassign", "Unassign profile (TRUE, FALSE)" },
  { GIMP_PDB_STRING, "profile", "ICC profile (empty:workspace)" }
};

static const GimpParamDef convert_args[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "(unused)" },
  { GIMP_PDB_STRING, "src-profile", "(unused in this version, should be empty)" },
  { GIMP_PDB_STRING, "dest-profile", "Destination profile (empty:workspace)" },
  { GIMP_PDB_INT32, "intent", "0:Perceptual, 1:Rel. Colorimetric, 2:Saturation, 3-4:Abs. Colorimetric" },
  { GIMP_PDB_INT32, "use_bpc", "Use BPC algorithm (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "use_dither", "Use dither (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "flatten", "Flatten the image before converting (TRUE, FALSE)" }
};

static const GimpParamDef abstract_args[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "(unused)" },
  { GIMP_PDB_STRING, "abstract-profile", "Abstract profile" },
  { GIMP_PDB_INT32, "intent", "0:Perceptual, 1:Rel. Colorimetric, 2:Saturation, 3-4:Abs. Colorimetric" },
  { GIMP_PDB_INT32, "use_bpc", "Use BPC algorithm (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "use_dither", "Use dither (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "target", "0:Selection, 1:Each layer, 2:Flatten image" }
};

static gint n_assign_args;
static gint n_convert_args;
static gint n_abstract_args;


MAIN ()


static void
query (void)
{
  /* setup for localization */
  INIT_I18N ();

  n_assign_args = sizeof(assign_args) / sizeof(assign_args[0]);
  n_convert_args = sizeof(convert_args) / sizeof(convert_args[0]);
  n_abstract_args = sizeof(abstract_args) / sizeof(abstract_args[0]);

  gimp_install_procedure ("plug-in-icc-colorspace-assign",
                          _("Assign new colorspace"),
                          _("Assign, replace or remove the ICC profile."),
                          "Yoshinori Yamakawa",
                          "Yoshinori Yamakawa",
                          "2007-2010",
                          N_("Assign colorspace..."),
                          "RGB*, INDEXED*",
                          GIMP_PLUGIN,
                          n_assign_args, 0,
                          assign_args, NULL);

  gimp_install_procedure ("plug-in-icc-colorspace-convert",
                          _("Convert to other colorspace"),
                          _("Convert to other RGB colorspace using ICC "
                            "profiles."),
                          "Yoshinori Yamakawa",
                          "Yoshinori Yamakawa",
                          "2007-2010",
                          N_("Convert colorspace..."),
                          "RGB*, INDEXED*",
                          GIMP_PLUGIN,
                          n_convert_args, 0,
                          convert_args, NULL);

  gimp_install_procedure ("plug-in-icc-colorspace-apply-abstract-profile",
                          _("Apply abstract profile"),
                          _("Apply the abstract profile for color effects."),
                          "Yoshinori Yamakawa",
                          "Yoshinori Yamakawa",
                          "2010",
                          N_("Apply abstract profile..."),
                          "RGB*, INDEXED*",
                          GIMP_PLUGIN,
                          n_abstract_args, 0,
                          abstract_args, NULL);

  gimp_plugin_menu_register ("plug-in-icc-colorspace-assign", "<Image>/Image/Mode");
  gimp_plugin_menu_register ("plug-in-icc-colorspace-convert", "<Image>/Image/Mode");
  gimp_plugin_menu_register ("plug-in-icc-colorspace-apply-abstract-profile", "<Image>/Colors");

  gimp_plugin_domain_register (GETTEXT_PACKAGE, NULL);
}


static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  IccColorspaceContext context;

  n_assign_args = sizeof(assign_args) / sizeof(assign_args[0]);
  n_convert_args = sizeof(convert_args) / sizeof(convert_args[0]);
  n_abstract_args = sizeof(abstract_args) / sizeof(abstract_args[0]);

  run_mode = context.run_mode = param[0].data.d_int32;

  context.imageID = param[1].data.d_image;

  if (strcmp (name, "plug-in-icc-colorspace-assign") == 0)
    context.func = ICC_COLORSPACE_ASSIGN;
  else if (strcmp (name, "plug-in-icc-colorspace-convert") == 0)
    context.func = ICC_COLORSPACE_CONVERT;
  else if (strcmp (name, "plug-in-icc-colorspace-apply-abstract-profile") == 0)
    context.func = ICC_COLORSPACE_ABSTRACT;
  else
    context.func = ICC_COLORSPACE_NONE;

  /* setup for localization */
  INIT_I18N ();

  lcms_error_setup ();

  icc_colorspace_init (&context);

  if (context.func != ICC_COLORSPACE_NONE)
    {
      switch (run_mode)
        {
        case GIMP_RUN_NONINTERACTIVE:
          switch (context.func)
            {
            case ICC_COLORSPACE_ASSIGN:
              if (nparams != n_assign_args)
                status = GIMP_PDB_CALLING_ERROR;
              else
                {
                  if (param[3].data.d_int32)
                    context.as.mode = ICC_COLORSPACE_ASSIGN_NONE;
                  else
                    {
                      context.filename = g_strdup (param[4].data.d_string);
                      context.as.mode = context.filename[0] == '\0' ? ICC_COLORSPACE_ASSIGN_WORKSPACE : ICC_COLORSPACE_ASSIGN_PROFILE;
                    }
                }

              break;
            case ICC_COLORSPACE_CONVERT:
              if (nparams != n_convert_args)
                status = GIMP_PDB_CALLING_ERROR;
              else
                {
                  context.filename         = param[4].data.d_string ? g_strdup (param[4].data.d_string) : g_strdup ("");
                  context.cs.use_workspace = (context.filename[0] == '\0');
                  context.cs.intent        = param[5].data.d_int32;
                  context.cs.bpc           = param[6].data.d_int32 ? TRUE : FALSE;
                  context.cs.dither        = param[7].data.d_int32 ? TRUE : FALSE;
                  context.cs.target        = param[8].data.d_int32 ? ICC_COLORSPACE_TARGET_FLATTEN_IMAGE : ICC_COLORSPACE_TARGET_LAYERS;
                }

              break;
            case ICC_COLORSPACE_ABSTRACT:
              if (nparams != n_abstract_args)
                status = GIMP_PDB_CALLING_ERROR;
              else
                {
                  context.filenames[0] = g_strdup (param[3].data.d_string);
                  context.n_filenames  = 1;
                  context.cs.intent    = param[4].data.d_int32;
                  context.cs.bpc       = param[5].data.d_int32 ? TRUE : FALSE;
                  context.cs.dither    = param[6].data.d_int32 ? TRUE : FALSE;
                  context.cs.target    = param[7].data.d_int32;
                }

              break;
            }

          break;
        case GIMP_RUN_INTERACTIVE:
          if (!(context.func == ICC_COLORSPACE_ASSIGN && assign_dialog (&context)) &&
              !(context.func == ICC_COLORSPACE_CONVERT && convert_dialog (&context)) &&
              !(context.func == ICC_COLORSPACE_ABSTRACT && abstract_dialog (&context)))
            status = GIMP_PDB_CANCEL;
          break;
        case GIMP_RUN_WITH_LAST_VALS:
        default:
          break;
        }
    }

  /* Check parameters */
  if (status != GIMP_PDB_CANCEL)
    {
      switch (context.func)
        {
        case ICC_COLORSPACE_ASSIGN:
          break;
        case ICC_COLORSPACE_CONVERT:
          if (context.cs.intent < 0 || context.cs.intent > INTENT_ABSOLUTE_COLORIMETRIC + 1)
            {
              gimp_message (_("Rendering intent is invalid."));
              status = GIMP_PDB_CALLING_ERROR;
            }
          break;
        case ICC_COLORSPACE_ABSTRACT:
          if (context.cs.target < 0 || context.cs.target > ICC_COLORSPACE_TARGET_FLATTEN_IMAGE)
            {
              gimp_message (_("Target is unknown."));
              status = GIMP_PDB_CALLING_ERROR;
            }
          else if (context.cs.intent < 0 || context.cs.intent > INTENT_ABSOLUTE_COLORIMETRIC + 1)
            {
              gimp_message (_("Rendering intent is invalid."));
              status = GIMP_PDB_CALLING_ERROR;
            }
          else if (!context.filenames[0] || context.filenames[0][0] == '\0')
            {
              gimp_message (_("Abstract profile is required."));
              status = GIMP_PDB_CALLING_ERROR;
            }
          break;
        case ICC_COLORSPACE_NONE:
        default:
          status = GIMP_PDB_CALLING_ERROR;
        }
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      gimp_image_undo_group_start (context.imageID);

      if (!(context.func == ICC_COLORSPACE_ASSIGN ?
            icc_colorspace_assign (&context) :
            icc_colorspace_convert (&context)))
        status = GIMP_PDB_EXECUTION_ERROR;

      gimp_image_undo_group_end (context.imageID);
    }

  if (status == GIMP_PDB_SUCCESS && run_mode == GIMP_RUN_INTERACTIVE)
    icc_colorspace_store_settings (&context);

  icc_colorspace_cleanup (&context);

  *nreturn_vals = 1;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  *return_vals = values;

  return;
}


static void
icc_colorspace_init (IccColorspaceContext *context)
{
  GimpParasite *parasite;
  gint32 imageID;
  GimpRunMode run_mode;
  IccColorspaceFunction func;
  gsize size;

#ifdef ENABLE_COLOR_MANAGEMENT
  GimpColorConfig *config;
#endif

  if (context == NULL)
    return;

  imageID = context->imageID;
  run_mode = context->run_mode;
  func = context->func;

  memset (context, '\0', sizeof(IccColorspaceContext));

  context->imageID = imageID;
  context->run_mode = run_mode;
  context->func = func;
  context->type = gimp_image_base_type (context->imageID);

  /* Set default values / Read last values */
  if (context->run_mode != GIMP_RUN_NONINTERACTIVE)
    {
      switch (func)
        {
        case ICC_COLORSPACE_ASSIGN:
          if ((size = gimp_get_data_size ("icc_colorspace_assign_settings")) == sizeof(context->as))
            gimp_get_data ("icc_colorspace_assign_settings", &context->as);

          if ((size = gimp_get_data_size ("icc_colorspace_assign_profile")))
            {
              g_free (context->filename);
              context->filename = g_new (gchar, size);
              gimp_get_data ("icc_colorspace_assign_profile", context->filename);
            }
          break;
        case ICC_COLORSPACE_CONVERT:
          if ((size = gimp_get_data_size ("icc_colorspace_convert_settings")) == sizeof(context->cs))
            gimp_get_data ("icc_colorspace_convert_settings", &context->cs);

          if ((size = gimp_get_data_size ("icc_colorspace_convert_destination_profile")))
            {
              g_free (context->filename);
              context->filename = g_new (gchar, size);
              gimp_get_data ("icc_colorspace_convert_destination_profile", context->filename);
            }
          else
            context->cs.use_workspace = TRUE;
          break;
        case ICC_COLORSPACE_ABSTRACT:
          if ((size = gimp_get_data_size ("icc_colorspace_abstract_settings")) == sizeof(AbstractSettings))
            gimp_get_data ("icc_colorspace_abstract_settings", &context->cs);
          else
            {
              context->cs.intent = GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC;
              context->cs.bpc    = TRUE;
            }

          if ((size = gimp_get_data_size ("icc_colorspace_abstract_profile")))
            {
              context->filenames[0] = g_new (gchar, size);
              gimp_get_data ("icc_colorspace_abstract_profile", context->filenames[0]);
              context->n_filenames = 1;
            }

          break;
        case ICC_COLORSPACE_NONE:
        default:
          break;
        }
    }

  /* Read embedded profile */
  if ((parasite = gimp_image_parasite_find (imageID, "icc-profile")))
    {
      context->profileSize = gimp_parasite_data_size (parasite);
      context->profile = g_memdup (gimp_parasite_data (parasite), context->profileSize);

      if (!(context->hProfile = cmsOpenProfileFromMem (context->profile, context->profileSize)))
        {
          g_free (context->profile);
          context->profile = NULL;
          context->profileSize = 0;
        }

      gimp_parasite_free (parasite);
    }

  /* Read workspace profile */
#ifdef ENABLE_COLOR_MANAGEMENT
  if ((config = gimp_get_color_configuration ()))
    {
      gchar *filename = NULL;

      g_object_get (config, "rgb-profile", &filename, NULL);

      if (filename)
        {
          if (g_file_get_contents (filename,
                                   &(context->workspaceProfile),
                                   &(context->workspaceProfileSize),
                                   NULL))
            {
              if (!(context->hWorkspaceProfile = cmsOpenProfileFromMem (context->workspaceProfile,
                                                                        context->workspaceProfileSize)))
                {
                  g_free (context->workspaceProfile);
                  context->workspaceProfile = NULL;
                  context->workspaceProfileSize = 0;
                }
            }
          g_free (filename);
        }

      if (!context->hWorkspaceProfile)
        {
          context->workspaceProfileSize = sizeof(sRGB_profile) - 1;
          context->workspaceProfile = g_memdup (sRGB_profile, context->workspaceProfileSize);
          context->hWorkspaceProfile = cmsOpenProfileFromMem (context->workspaceProfile,
                                                              context->workspaceProfileSize);
        }
      g_object_unref (G_OBJECT (config));
  }
#else
  context->workspaceProfileSize = sizeof(sRGB_profile) - 1;
  context->workspaceProfile = g_memdup (sRGB_profile, context->workspaceProfileSize);
  context->hWorkspaceProfile = cmsOpenProfileFromMem (context->workspaceProfile,
                                                      context->workspaceProfileSize);
#endif /* ENABLE_COLOR_MANAGEMENT */

  return;
}

static void
icc_colorspace_cleanup (IccColorspaceContext *context)
{
  if (context->hProfile)
    cmsCloseProfile (context->hProfile);
  g_free (context->profile);

  if (context->hWorkspaceProfile)
    cmsCloseProfile (context->hWorkspaceProfile);
  g_free (context->workspaceProfile);

  g_free (context->filename);

  if (context->hTransform)
    cmsDeleteTransform (context->hTransform);
  if (context->hTransformA)
    cmsDeleteTransform (context->hTransformA);
}

static void
icc_colorspace_store_settings (IccColorspaceContext *context)
{
  switch (context->func)
    {
    case ICC_COLORSPACE_ASSIGN:
      if (context->filename)
        gimp_set_data ("icc_colorspace_assign_profile", context->filename, strlen (context->filename) + 1);
      gimp_set_data ("icc_colorspace_assign_settings", &context->as, sizeof(context->as));
      break;
    case ICC_COLORSPACE_CONVERT:
      if (context->filename)
        gimp_set_data ("icc_colorspace_convert_destination_profile", context->filename, strlen (context->filename) + 1);
      gimp_set_data ("icc_colorspace_convert_settings", &context->cs, sizeof(context->cs));
      break;
    case ICC_COLORSPACE_ABSTRACT:
      if (context->filenames[0])
        gimp_set_data ("icc_colorspace_abstract_profile", context->filenames[0], strlen (context->filenames[0]) + 1);
      gimp_set_data ("icc_colorspace_abstract_settings", &context->cs, sizeof(AbstractSettings));
      break;
    case ICC_COLORSPACE_NONE:
    default:
      break;
    }
}


static inline gboolean
icc_colorspace_compare_profile (const gpointer ptr1,
                                gsize          size1,
                                const gpointer ptr2,
                                gsize          size2)
{
  if (size1 != size2 || size1 <= sizeof(icHeader) || size2 <= sizeof(icHeader) ||
      memcmp (ptr1 + sizeof(icHeader), ptr2 + sizeof(icHeader), size1 - sizeof(icHeader)) != 0)
    return FALSE;
  else
    return TRUE;
}

static gboolean
_xyzcmp (cmsCIEXYZ v1,
         cmsCIEXYZ v2)
{
#ifdef ICC_COLORSPACE_DEBUG
  gchar *str = g_strdup_printf ("{ %f, %f, %f } { %f, %f, %f }", v1.X, v1.Y, v1.Z, v2.X, v2.Y, v2.Z);
  gimp_message (str);
  g_free (str);
#endif
  if (fabs (v1.X - v2.X) <= 0.001 &&
      fabs (v1.Y - v2.Y) <= 0.001 &&
      fabs (v1.Z - v2.Z) <= 0.001)
    return TRUE;
  else
    return FALSE;
}

static gboolean
icc_colorspace_compare_display_profile (cmsHPROFILE hProfile1,
                                        cmsHPROFILE hProfile2)
{
  const icTagSignature trcTags[3] = { icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag };

  if (hProfile1 && hProfile2 &&
      cmsGetDeviceClass (hProfile1) == icSigDisplayClass &&
      cmsGetDeviceClass (hProfile2) == icSigDisplayClass)
    {
      int i;
      cmsCIEXYZTRIPLE xyz1, xyz2;
      LPGAMMATABLE gamma1, gamma2;
      gboolean result = TRUE;

      xyz1.Red = lcms_get_whitepoint (hProfile1);
      xyz2.Red = lcms_get_whitepoint (hProfile2);
      if (!_xyzcmp (xyz1.Red, xyz2.Red))
        return FALSE;

      xyz1 = lcms_get_colorants (hProfile1);
      xyz2 = lcms_get_colorants (hProfile2);
      if (!_xyzcmp (xyz1.Red, xyz2.Red) ||
          !_xyzcmp (xyz1.Green, xyz2.Green) ||
          !_xyzcmp (xyz1.Blue, xyz2.Blue))
        return FALSE;

      for (i = 0; i < 3 && result == TRUE; i++)
        {
          gamma1 = lcms_get_gamma (hProfile1, trcTags[i]);
          gamma2 = lcms_get_gamma (hProfile2, trcTags[i]);
          if (floor (lcms_estimate_gamma (gamma1) * 10 + 0.5) != floor (lcms_estimate_gamma (gamma2) * 10 + 0.5))
            result = FALSE;

          if (gamma1)
            lcms_free_gamma (gamma1);
          if (gamma2)
            lcms_free_gamma (gamma2);
        }

      return result;
    }
  else
    return FALSE;
}


static gboolean
icc_colorspace_assign (IccColorspaceContext *context)
{
  gchar *buf = NULL;
  gsize length = 0;
  GimpParasite *parasite = NULL;

  switch (context->as.mode)
    {
    case ICC_COLORSPACE_ASSIGN_NONE:
      break;
    case ICC_COLORSPACE_ASSIGN_WORKSPACE:
      /* Profile is embedded, and cannot be used independently */
      context->workspaceProfile[47] |= 2;

      parasite = gimp_parasite_new ("icc-profile", PARASITE_FLAGS,
                                    context->workspaceProfileSize,
                                    context->workspaceProfile);
      break;
    case ICC_COLORSPACE_ASSIGN_PROFILE:
      if (context->filename)
        {
          if (g_file_get_contents (context->filename, &buf, &length, NULL))
            {
              /* Profile is embedded, and cannot be used independently */
              buf[47] |= 2;

              parasite = gimp_parasite_new ("icc-profile", PARASITE_FLAGS, length, buf);
            }
          else
            return FALSE;
        }
      else
        return FALSE;
    }

  if (parasite)
    {
      gimp_image_parasite_attach (context->imageID, parasite);
      gimp_parasite_free (parasite);
    }
  else
    gimp_image_parasite_detach (context->imageID, "icc-profile");

  g_free (buf);

  /* Redraw displays */
  {
    gint i, nLayers;
    gint32 *layerID;
    gint offsetX, offsetY;

    layerID = gimp_image_get_layers (context->imageID, &nLayers);

    for (i = 0; i < nLayers && !gimp_drawable_get_visible (layerID[i]); i++) ;

    if (i < nLayers)
      {
        gimp_drawable_offsets (layerID[i], &offsetX, &offsetY);

        gimp_drawable_update (layerID[i], -offsetX, -offsetY,
                              gimp_image_width (context->imageID),
                              gimp_image_height (context->imageID));

        gimp_displays_flush ();
      }

    g_free (layerID);
  }

  return TRUE;
}

static gboolean
icc_colorspace_convert (IccColorspaceContext *context)
{
  gboolean return_val = TRUE;

  gchar *srcProfile = NULL;
  gsize srcProfileSize = 0;
  /*cmsHPROFILE hSrcProfile = NULL;*/

  gchar *destProfile = NULL;
  gsize destProfileSize = 0;
  cmsHPROFILE hDestProfile = NULL; /* クローズのために使用 */

  cmsHPROFILE profiles[16 + 2] = {0};
  gint n_profiles;

  gboolean skip = FALSE;

#ifdef ENABLE_BENCHMARK
  GString *msg;

  msg = g_string_new ("");
  context->timer1 = g_timer_new ();
#endif

  /* Setup the source profile */
  if (context->hProfile)
    {
      srcProfile = context->profile;
      srcProfileSize = context->profileSize;
      /*hSrcProfile = context->hProfile;*/
      profiles[0] = context->hProfile;
    }
  else
    {
      srcProfile = context->workspaceProfile;
      srcProfileSize = context->workspaceProfileSize;
      /*hSrcProfile = context->hWorkspaceProfile;*/
      profiles[0] = context->hWorkspaceProfile;
    }

  /* 間に挟むプロファイルの読み込み */
  for (n_profiles = 1; n_profiles <= context->n_filenames; n_profiles++)
    {
      profiles[n_profiles] = lcms_open_profile (context->filenames[n_profiles - 1]);

      if (!profiles[n_profiles])
        {
          gimp_message ("Cannot open the profile(s).");

          skip = TRUE;
          return_val = FALSE;

          break;
        }
    }

  /* Setup the destination profile */
  if (context->func == ICC_COLORSPACE_ABSTRACT)
    profiles[n_profiles++] = profiles[0]; /* destination = source */
  else if (context->cs.use_workspace)
    {
      if (context->n_filenames == 0 &&
          icc_colorspace_compare_profile (srcProfile, srcProfileSize,
                                          context->workspaceProfile, context->workspaceProfileSize))
        {
          gimp_message ("Two profiles are same.\nConversion is skipped.");

          skip = TRUE; /* not need conversion */
        }
      else
        {
          profiles[n_profiles++] = context->hWorkspaceProfile;
          destProfile = context->workspaceProfile;
          destProfileSize = context->workspaceProfileSize;
        }
    }
  else if (g_file_get_contents (context->filename, &destProfile, &destProfileSize, NULL))
    {
      if (context->n_filenames == 0 &&
          icc_colorspace_compare_profile (srcProfile, srcProfileSize,
                                          destProfile, destProfileSize))
        {
          gimp_message ("Two profiles are same.\nConversion is skipped.");

          skip = TRUE;
        }
      else if ((hDestProfile = cmsOpenProfileFromMem (destProfile, destProfileSize)))
        profiles[n_profiles++] = hDestProfile;
      else
        {
          gimp_message ("Cannot open the destination profile.");

          skip = TRUE;
          return_val = FALSE;
        }
    }
  else
    {
      skip = TRUE;
      return_val = FALSE; /* Can't read destination profile */
    }

  if (!skip && (n_profiles > 2 || !icc_colorspace_compare_display_profile (profiles[0], profiles[1])))
    {
      /* Setup transform */
      ICCRenderingIntent intent = context->cs.intent;
      DWORD format = TYPE_RGB_8;
      DWORD dwFLAGS = 0;

      /* Photoshop CS2 (and above) compatibility */
      if (intent == INTENT_ABSOLUTE_COLORIMETRIC + 1)
        {
          cmsSetAdaptationState (1.0);
          intent = INTENT_ABSOLUTE_COLORIMETRIC;
        }
      else
        cmsSetAdaptationState (0);

      /* Other settings */
      if (context->cs.bpc)
        dwFLAGS |= cmsFLAGS_BLACKPOINTCOMPENSATION;
      if (context->func == ICC_COLORSPACE_ABSTRACT || context->cs.intent == INTENT_ABSOLUTE_COLORIMETRIC)
        dwFLAGS |= cmsFLAGS_NOWHITEONWHITEFIXUP;
      if (context->cs.dither && context->type != GIMP_INDEXED)
        format |= DITHER_SH (1);

#ifdef ENABLE_BENCHMARK
      context->timer2 = g_timer_new ();
#endif

      context->hTransform  = cmsCreateMultiprofileTransform (profiles, n_profiles,
                                                             format, format,
                                                             intent, dwFLAGS);
      format |= EXTRA_SH (1);
      context->hTransformA = cmsCreateMultiprofileTransform (profiles, n_profiles,
                                                             format, format,
                                                             intent, dwFLAGS);

#ifdef ENABLE_BENCHMARK
      g_string_printf (msg, "LCMS header version : %d\nTransform setup : %fsec.",
                       LCMS_VERSION,
                       g_timer_elapsed (context->timer2, NULL));
      g_timer_start (context->timer2);
      g_timer_stop (context->timer2);
      g_timer_reset (context->timer2);
#endif

      if (context->hTransform && context->hTransformA)
        {
          gint32 *layerID;
          gint32 selectionID = -1;
          gint i, nLayers, nTiles;
          GimpDrawable **drawables;

          if (context->cs.target == ICC_COLORSPACE_TARGET_FLATTEN_IMAGE)
            gimp_image_flatten (context->imageID);

          gimp_progress_init (_("Conveting..."));

          /* Get layer IDs */
          if (context->cs.target == ICC_COLORSPACE_TARGET_SELECTION)
            {
              layerID = g_new (gint32, 1);
              *layerID = gimp_image_get_active_layer (context->imageID);
              nLayers = 1;
            }
          else
            {
              /* unselect temporarily */
              if (!gimp_selection_is_empty (context->imageID))
                {
                  selectionID = gimp_selection_save (context->imageID);
                  gimp_selection_none (context->imageID);
                }

              layerID = gimp_image_get_layers (context->imageID, &nLayers);
            }
          drawables = (GimpDrawable **)g_new (GimpDrawable, nLayers);

          if (context->type == GIMP_INDEXED)
            {
              /* Get drawables */
              for (i = 0, nTiles = 0; i < nLayers; i++)
                drawables[i] = gimp_drawable_get (layerID[i]);

              /* Do transform... */
              return_val = icc_colorspace_convert_colormap (context);
            }
          else
            {
              /* Get drawables and the number of tiles */
              for (i = 0, nTiles = 0; i < nLayers; i++)
                {
                  drawables[i] = gimp_drawable_get (layerID[i]);
                  nTiles += drawables[i]->ntile_rows * drawables[i]->ntile_cols;
                }
              context->step = 1.0 / nTiles;

              /* Do transform... */
              for (i = 0; i < nLayers; i++)
                return_val = icc_colorspace_convert_layer (context, drawables[i], (context->cs.target == ICC_COLORSPACE_TARGET_SELECTION)) ? return_val : FALSE;
            }
#ifdef ENABLE_BENCHMARK
          g_string_append_printf (msg, "\nApplying transform : %fsec.", g_timer_elapsed (context->timer2, NULL));
          g_timer_destroy (context->timer2);
#endif

          /* Embed destination profile */
          /* Profile is embedded, and cannot be used independently */
          if (destProfile)
            {
              destProfile[47] |= 2;

#if GIMP_MAJOR_VERSION > 2 || (GIMP_MAJOR_VERSION == 2 && GIMP_MINOR_VERSION > 3)
              return_val = gimp_image_attach_new_parasite (context->imageID, "icc-profile",
                                                           PARASITE_FLAGS,
                                                           destProfileSize, destProfile) ? return_val : FALSE;

#else
              gimp_image_attach_new_parasite (context->imageID, "icc-profile",
                                              PARASITE_FLAGS,
                                              destProfileSize, destProfile);
#endif
            }

          /* Finishing */
          for (i = 0; i < nLayers; i++)
            {
              gimp_drawable_update (layerID[i], 0, 0, drawables[i]->width, drawables[i]->height);
              gimp_drawable_detach (drawables[i]);
            }

          if (selectionID != -1)
            {
              gimp_selection_load (selectionID);
              gimp_image_remove_channel (context->imageID, selectionID);
            }

          g_free (drawables);
          g_free (layerID);

          gimp_progress_update (1.0);
        }
      else
        return_val = FALSE;
    }
  else
    {
      if (!skip)
        gimp_message ("Two colorspaces seem to be same.\nConversion is skipped.");

      if (context->cs.target == ICC_COLORSPACE_TARGET_FLATTEN_IMAGE)
        gimp_image_flatten (context->imageID);

      /* Embed destination profile if image has no profiles */
      if ((!context->hProfile || !skip) && destProfile)
        {
          /* Profile is embedded, and cannot be used independently */
          destProfile[47] |= 2;

#if GIMP_MAJOR_VERSION > 2 || (GIMP_MAJOR_VERSION == 2 && GIMP_MINOR_VERSION > 3)
          return_val = gimp_image_attach_new_parasite (context->imageID, "icc-profile",
                                                       PARASITE_FLAGS,
                                                       destProfileSize, destProfile) ? return_val : FALSE;
#else
          gimp_image_attach_new_parasite (context->imageID, "icc-profile",
                                          PARASITE_FLAGS,
                                          destProfileSize, destProfile);
#endif
        }
    }

  if (destProfile)
    {
      if (hDestProfile)
        cmsCloseProfile (hDestProfile);
      g_free (destProfile);
    }

  n_profiles--;
  while (n_profiles > 1)
    {
      /* Don't close src/dest profiles! */
      n_profiles--;
      cmsCloseProfile (profiles[n_profiles]);
    }

  gimp_displays_flush ();

#ifdef ENABLE_BENCHMARK
  g_string_append_printf (msg, "\nPlugin execution time : %fsec.", g_timer_elapsed (context->timer1, NULL));
  gimp_message (msg->str);
  g_string_free (msg, TRUE);
  g_timer_destroy (context->timer1);
#endif

  return return_val;
}

static gboolean
icc_colorspace_convert_colormap (IccColorspaceContext *context)
{
  guchar *colorMap;
  gint nColors;

  if ((colorMap = gimp_image_get_colormap (context->imageID, &nColors)) == NULL)
    {
      gimp_message (_("Can't get colormap!"));
      return FALSE;
    }

#ifdef ENABLE_BENCHMARK
  g_timer_continue (context->timer2);
#endif

  cmsDoTransform (context->hTransform, colorMap, colorMap, nColors);

#ifdef ENABLE_BENCHMARK
  g_timer_stop (context->timer2);
#endif

  return gimp_image_set_colormap (context->imageID, colorMap, nColors);
}

static gboolean
icc_colorspace_convert_layer (IccColorspaceContext *context,
                              GimpDrawable         *drawable,
                              gboolean              selection)
{
  gint32 layerID;
  GimpPixelRgn region_in, region_out;
  gpointer iterator;
  gint x1, y1, x2, y2, width, height;

  gint bpp;
  cmsHTRANSFORM transform;

  layerID = drawable->drawable_id;
  bpp = gimp_drawable_bpp (layerID);

  if (bpp == 4)
    transform = context->hTransformA;
  else if (bpp == 3)
    transform = context->hTransform;
  else
    {
      gimp_message (_("Unsupported BPP!"));
      return FALSE;
    }

  if (selection)
    {
      gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
      width = (x2 - x1);
      height = (y2 - y1);
    }
  else
    {
      x1 = y1 = 0;
      width = drawable->width;
      height = drawable->height;
    }
  gimp_pixel_rgn_init (&region_in,  drawable, x1, y1, width, height, TRUE, FALSE); /* "FALSE, FALSE" is bad. */
  gimp_pixel_rgn_init (&region_out, drawable, x1, y1, width, height, TRUE, TRUE);

  iterator = gimp_pixel_rgns_register (2, &region_in, &region_out);

  while (iterator != NULL)
    {
      guchar *in = region_in.data, *out = region_out.data;
      gint i;

      /* Copy alpha channel */
      if (bpp == 4)
        g_memmove (region_out.data, region_in.data, region_in.rowstride * region_in.h);


#ifdef ENABLE_BENCHMARK
      g_timer_continue (context->timer2);
#endif

      for (i = 0; i < region_in.h; i++, in += region_in.rowstride, out += region_out.rowstride)
        cmsDoTransform (transform, in, out, region_in.w);

#ifdef ENABLE_BENCHMARK
      g_timer_stop (context->timer2);
#endif

      context->percentage += context->step;
      gimp_progress_update (context->percentage);

      iterator = gimp_pixel_rgns_process (iterator);
    }

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (layerID, TRUE);

  return TRUE;
}

static void
icc_button_changed (IccButton      *button,
                    GtkRadioButton *radioButton)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton), TRUE);
}

static gboolean
assign_dialog (IccColorspaceContext *context)
{
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *arrow;
  GtkWidget *hbox, *vbox1, *vbox2;
  GtkWidget *destProfile;
  GtkWidget *radioButton[3];

  gchar *profileDesc, *string;

  gboolean isOK;

  gimp_ui_init ("icc_colorspace", FALSE);

  dialog = gimp_dialog_new (_("Assign colorspace"), "assign-colorspace",
                            NULL, 0,
                            gimp_standard_help_func, "gimp-filter-assign-colorspace",
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);
  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox1, TRUE, TRUE, 0);


  frame = gtk_frame_new (_("Current colorspace:"));
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  profileDesc = lcms_get_profile_desc (context->hProfile);
  label = gtk_label_new (profileDesc ? profileDesc : _("Unknown (image has no profiles)"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
  if (!profileDesc)
    {
      label = gtk_label_new ("");
      gtk_label_set_markup (GTK_LABEL (label), _("<span size=\"smaller\" style=\"italic\">plug-in uses RGB workspace temporarily</span>"));
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
    }
  g_free (profileDesc);


  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox1), arrow, TRUE, TRUE, 0);


  frame = gtk_frame_new (_("New colorspace:"));
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  radioButton[0] = gtk_radio_button_new_with_mnemonic (NULL, _("_Unassign colorspace"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton[0]), (context->as.mode == ICC_COLORSPACE_ASSIGN_NONE));
  gtk_box_pack_start (GTK_BOX (vbox2), radioButton[0], TRUE, TRUE, 0);

  profileDesc = lcms_get_profile_desc (context->hWorkspaceProfile);
  string = g_strdup_printf (_("_Workspace: %s"), profileDesc ? profileDesc : "sRGB");
  radioButton[1] = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radioButton[0]), string);
  g_free (profileDesc);
  g_free (string);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton[1]), (context->as.mode == ICC_COLORSPACE_ASSIGN_WORKSPACE));
  gtk_box_pack_start (GTK_BOX (vbox2), radioButton[1], TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

  radioButton[2] = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radioButton[0]), _("_Profile:"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton[2]), (context->as.mode == ICC_COLORSPACE_ASSIGN_PROFILE));
  gtk_box_pack_start (GTK_BOX (hbox), radioButton[2], FALSE, TRUE, 0);

  destProfile = icc_button_new ();
  icc_button_set_max_entries (ICC_BUTTON (destProfile), 10);
  icc_button_set_title (ICC_BUTTON (destProfile), _("Choose RGB profile"));
  icc_button_set_mask (ICC_BUTTON (destProfile), ICC_BUTTON_CLASS_INPUT | ICC_BUTTON_CLASS_OUTPUT | ICC_BUTTON_CLASS_DISPLAY, ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_RGB);
  icc_button_set_filename (ICC_BUTTON (destProfile), context->filename, FALSE);
  icc_button_set_enable_empty (ICC_BUTTON (destProfile), FALSE);
  icc_button_dialog_set_show_detail (ICC_BUTTON (destProfile), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (destProfile), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  g_signal_connect (G_OBJECT (destProfile), "changed", G_CALLBACK (icc_button_changed), radioButton[2]);
  gtk_box_pack_start (GTK_BOX (hbox), destProfile, TRUE, TRUE, 0);

  gtk_widget_show_all (dialog);

  isOK = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  if (isOK)
    {
      g_free (context->filename);

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[2])))
        context->filename = icc_button_get_filename (ICC_BUTTON (destProfile));
      else
        context->filename = NULL;

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[0])))
        context->as.mode = ICC_COLORSPACE_ASSIGN_NONE;
      else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[1])))
        context->as.mode = ICC_COLORSPACE_ASSIGN_WORKSPACE;
      else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[2])))
        context->as.mode = ICC_COLORSPACE_ASSIGN_PROFILE;
    }

  gtk_widget_destroy (dialog);

  return isOK;
}

static gboolean
convert_dialog (IccColorspaceContext *context)
{
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *arrow;
  GtkWidget *hbox, *vbox1, *vbox2;
  GtkWidget *destProfile;
  GtkWidget *radioButton[2];
  GtkWidget *checkButton[3];
  GtkWidget *comboBox;

  gchar *profileDesc, *string;

  gboolean isOK;

  gimp_ui_init ("icc_colorspace", FALSE);

  dialog = gimp_dialog_new (_("Convert colorspace"), "convert-colorspace",
                            NULL, 0,
                            gimp_standard_help_func, "gimp-filter-convert-colorspace",
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);
  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox1, TRUE, TRUE, 0);

  frame = gtk_frame_new (_("Source colorspace:"));
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  profileDesc = lcms_get_profile_desc (context->hProfile);
  label = gtk_label_new (profileDesc ? profileDesc : _("Unknown (image has no profiles)"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
  if (!profileDesc)
    {
      label = gtk_label_new ("");
      gtk_label_set_markup (GTK_LABEL (label), _("<span size=\"smaller\" style=\"italic\">plug-in uses RGB workspace temporarily</span>"));
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0 );
    }
  g_free (profileDesc);


  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox1), arrow, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  label = gtk_label_new_with_mnemonic (_("_Rendering intent:"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  comboBox = gtk_combo_box_new_text ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), comboBox);
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Perceptual"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Relative colorimetric"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Saturation"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Absolute colorimetric"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Absolute colorimetric(2)"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (comboBox), context->cs.intent);
  gtk_box_pack_start (GTK_BOX (vbox2), comboBox, FALSE, TRUE, 0);

  checkButton[0] = gtk_check_button_new_with_mnemonic (_("Use _BPC algorithm"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[0]), context->cs.bpc);
  gtk_box_pack_start (GTK_BOX (vbox2), checkButton[0], TRUE, TRUE, 0);

  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox1), arrow, TRUE, TRUE, 0);


  frame = gtk_frame_new (_("Destination colorspace:"));
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  profileDesc = lcms_get_profile_desc (context->hWorkspaceProfile);
  string = g_strdup_printf (_("_Workspace: %s"), profileDesc ? profileDesc : "sRGB");
  radioButton[0] = gtk_radio_button_new_with_mnemonic (NULL, string);
  g_free (profileDesc);
  g_free (string);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton[0]), context->cs.use_workspace);
  gtk_box_pack_start (GTK_BOX (vbox2), radioButton[0], TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

  radioButton[1] = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radioButton[0]), _("_Profile:"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton[1]), !context->cs.use_workspace);
  gtk_box_pack_start (GTK_BOX (hbox), radioButton[1], FALSE, TRUE, 0);

  destProfile = icc_button_new ();
  icc_button_set_max_entries (ICC_BUTTON (destProfile), 10);
  icc_button_set_title (ICC_BUTTON (destProfile), _("Choose RGB profile"));
  icc_button_set_mask (ICC_BUTTON (destProfile), ICC_BUTTON_CLASS_OUTPUT | ICC_BUTTON_CLASS_DISPLAY, ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_RGB);
  icc_button_set_filename (ICC_BUTTON (destProfile), context->filename, FALSE);
  icc_button_set_enable_empty (ICC_BUTTON (destProfile), FALSE);
  icc_button_dialog_set_show_detail (ICC_BUTTON (destProfile), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (destProfile), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  g_signal_connect (G_OBJECT (destProfile), "changed", G_CALLBACK (icc_button_changed), radioButton[1]);
  gtk_box_pack_start (GTK_BOX (hbox), destProfile, TRUE, TRUE, 0);

  checkButton[1] = gtk_check_button_new_with_mnemonic (_("Use _dither"));
#ifdef ENABLE_DITHER
  gtk_widget_set_sensitive (checkButton[1], (context->type != GIMP_INDEXED));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[1]), context->cs.dither);
#else
  gtk_widget_set_sensitive (checkButton[1], FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[1]), FALSE);
#endif
  gtk_box_pack_start (GTK_BOX (vbox1), checkButton[1], TRUE, TRUE, 0);

  checkButton[2] = gtk_check_button_new_with_mnemonic (_("_Flatten image to preserve appearance"));
  gtk_widget_set_sensitive (checkButton[2], (context->type != GIMP_INDEXED));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[2]), (context->cs.target == ICC_COLORSPACE_TARGET_FLATTEN_IMAGE));
  gtk_box_pack_start (GTK_BOX (vbox1), checkButton[2], TRUE, TRUE, 0);

  gtk_widget_show_all (dialog);

  isOK = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  if (isOK)
    {
      g_free (context->filename);

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[1])))
        context->filename = icc_button_get_filename (ICC_BUTTON (destProfile));
      else
        context->filename = NULL;

      context->cs.use_workspace = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[0]));

      context->cs.intent = gtk_combo_box_get_active (GTK_COMBO_BOX (comboBox));
      context->cs.bpc = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton[0]));

      context->cs.dither = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton[1]));
      context->cs.target = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton[2])) ? ICC_COLORSPACE_TARGET_FLATTEN_IMAGE : ICC_COLORSPACE_TARGET_LAYERS;
    }

  gtk_widget_destroy (dialog);

  return isOK;
}

static void
absolute_is_ready (IccButton *button,
                   GtkDialog *dialog)
{
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK,
                                     !icc_button_is_empty (button));
}

static gboolean
abstract_dialog (IccColorspaceContext *context)
{
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *arrow;
  GtkWidget *hbox, *vbox1, *vbox2;
  GtkWidget *destProfile;
  GtkWidget *comboBox;
  GtkWidget *checkButton[2];
  GtkWidget *radioButton[3];

  gchar *profileDesc, *string;

  gboolean isOK;

  gimp_ui_init ("icc_colorspace", FALSE);

  dialog = gimp_dialog_new (_("Apply abstract profile"), "apply-abstract-profile",
                            NULL, 0,
                            gimp_standard_help_func, "plug-in-icc-colorspace-apply-abstract-profile",
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);
  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  vbox1 = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox1, TRUE, TRUE, 0);


  frame = gtk_frame_new (NULL);
  label = gtk_label_new_with_mnemonic (_("_Abstract profile:"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  destProfile = icc_button_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), destProfile);
  icc_button_set_max_entries (ICC_BUTTON (destProfile), 10);
  icc_button_set_title (ICC_BUTTON (destProfile), _("Choose abstract profile"));
  icc_button_set_mask (ICC_BUTTON (destProfile), ICC_BUTTON_CLASS_ABSTRACT, ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_ALL);
  icc_button_set_enable_empty (ICC_BUTTON (destProfile), FALSE);
  icc_button_dialog_set_show_detail (ICC_BUTTON (destProfile), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (destProfile), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  icc_button_set_filename (ICC_BUTTON (destProfile), context->filenames[0], FALSE);
  g_signal_connect (G_OBJECT (destProfile), "changed", G_CALLBACK (absolute_is_ready), dialog);
  gtk_box_pack_start (GTK_BOX (vbox2), destProfile, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  label = gtk_label_new_with_mnemonic (_("_Rendering intent:"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  comboBox = gtk_combo_box_new_text ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), comboBox);
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Perceptual"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Relative colorimetric"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Saturation"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Absolute colorimetric"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), _("Absolute colorimetric(2)"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (comboBox), context->cs.intent);
  gtk_box_pack_start (GTK_BOX (vbox2), comboBox, FALSE, TRUE, 0);

  checkButton[0] = gtk_check_button_new_with_mnemonic (_("Use _BPC algorithm"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[0]), context->cs.bpc);
  gtk_box_pack_start (GTK_BOX (vbox2), checkButton[0], TRUE, TRUE, 0);

  frame = gtk_frame_new (_("Target:"));
  gtk_widget_set_sensitive (frame, (context->type != GIMP_INDEXED));
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);

  radioButton[0] = gtk_radio_button_new_with_mnemonic (NULL, _("_Selection"));
  radioButton[1] = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radioButton[0]), _("Each _layer"));
  radioButton[2] = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radioButton[0]), _("_Flatten image"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radioButton[context->cs.target]), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox2), radioButton[0], TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), radioButton[1], TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), radioButton[2], TRUE, TRUE, 0);

  checkButton[1] = gtk_check_button_new_with_mnemonic (_("Use _dither"));
#ifdef ENABLE_DITHER
  gtk_widget_set_sensitive (checkButton[1], (context->type != GIMP_INDEXED));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[1]), context->cs.dither);
#else
  gtk_widget_set_sensitive (checkButton[1], FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkButton[1]), FALSE);
#endif
  gtk_box_pack_start (GTK_BOX (vbox1), checkButton[1], TRUE, TRUE, 0);


  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK,
                                     !icc_button_is_empty (ICC_BUTTON (destProfile)));

  gtk_widget_show_all (dialog);

  isOK = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  if (isOK)
    {
      gint i;

      g_free (context->filenames[0]);
      if ((context->filenames[0] = icc_button_get_filename (ICC_BUTTON (destProfile))))
          context->n_filenames = 1;

      context->cs.intent = gtk_combo_box_get_active (GTK_COMBO_BOX (comboBox));
      context->cs.bpc    = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton[0]));
      context->cs.dither = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkButton[1]));

      for (i = 0; i < 3; i++)
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radioButton[i])))
            context->cs.target = i;
        }
    }

  gtk_widget_destroy (dialog);

  return isOK;
}
