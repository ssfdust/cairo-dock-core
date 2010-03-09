/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <cairo.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-container.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-position.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-desklets.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-modules.h"

extern CairoDock *g_pMainDock;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCairoDockDataDir;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;
extern gboolean g_bEasterEggs;

static GHashTable *s_hModuleTable = NULL;
static GHashTable *s_hInternalModuleTable = NULL;
static int s_iMaxOrder = 0;
static GList *s_AutoLoadedModules = NULL;

static void _entered_help_once (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile)
{
	gchar *cHelpDir = g_strdup_printf ("%s/.help", g_cCairoDockDataDir);
	gchar *cHelpHistory = g_strdup_printf ("%s/entered-once", cHelpDir);
	if (! g_file_test (cHelpDir, G_FILE_TEST_EXISTS))
	{
		if (g_mkdir (cHelpDir, 7*8*8+7*8+5) != 0)
		{
			cd_warning ("couldn't create directory %s", cHelpDir);
			return;
		}
	}
	if (! g_file_test (cHelpHistory, G_FILE_TEST_EXISTS))
	{
		g_file_set_contents (cHelpHistory,
			"1",
			-1,
			NULL);
	}
	g_free (cHelpHistory);
	g_free (cHelpDir);
}

void cairo_dock_initialize_module_manager (const gchar *cModuleDirPath)
{
	if (s_hModuleTable == NULL)
		s_hModuleTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			NULL,  // la cle est le nom du module, et pointe directement sur le champ 'cModuleName' du module.
			(GDestroyNotify) cairo_dock_free_module);
	
	if (s_hInternalModuleTable == NULL)
	{
		s_hInternalModuleTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			NULL,  // la cle est le nom du module, et pointe directement sur le champ 'cModuleName' du module.
			(GDestroyNotify) NULL);  // ne sont jamais liberes.
		cairo_dock_preload_internal_modules (s_hInternalModuleTable);
	}
	
	if (cModuleDirPath != NULL && g_file_test (cModuleDirPath, G_FILE_TEST_IS_DIR))
	{
		GError *erreur = NULL;
		cairo_dock_preload_module_from_directory (cModuleDirPath, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("%s\n  no module will be available", erreur->message);
			g_error_free (erreur);
		}
	}
	
	//\________________ ceci est un vilain hack ... mais je trouvais ca lourd de compiler un truc qui n'a aucun code, et puis comme ca on a l'aide meme sans les plug-ins.
	CairoDockModule *pHelpModule = g_new0 (CairoDockModule, 1);
	CairoDockVisitCard *pVisitCard = g_new0 (CairoDockVisitCard, 1);
	pHelpModule->pVisitCard = pVisitCard;
	pVisitCard->cModuleName = "Help";
	pVisitCard->cTitle = _("Help");
	pVisitCard->iMajorVersionNeeded = 2;
	pVisitCard->iMinorVersionNeeded = 0;
	pVisitCard->iMicroVersionNeeded = 0;
	pVisitCard->cPreviewFilePath = NULL;
	pVisitCard->cGettextDomain = NULL;
	pVisitCard->cDockVersionOnCompilation = CAIRO_DOCK_VERSION;
	pVisitCard->cUserDataDir = "help";
	pVisitCard->cShareDataDir = CAIRO_DOCK_SHARE_DATA_DIR;
	pVisitCard->cConfFileName = "help.conf";
	pVisitCard->cModuleVersion = "0.0.8";
	pVisitCard->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	pVisitCard->cIconFilePath = CAIRO_DOCK_SHARE_DATA_DIR"/icon-help.svg";
	pVisitCard->iSizeOfConfig = 0;
	pVisitCard->iSizeOfData = 0;
	pVisitCard->cDescription = N_("A useful FAQ that contains also a lot of hints.\nLet the mouse over a sentence to make the hint dialog popups.");
	pVisitCard->cAuthor = "Fabounet";
	pHelpModule->pInterface = g_new0 (CairoDockModuleInterface, 1);
	pHelpModule->pInterface->load_custom_widget = _entered_help_once;
	g_hash_table_insert (s_hModuleTable, (gpointer)pHelpModule->pVisitCard->cModuleName, pHelpModule);
	cairo_dock_activate_module (pHelpModule, NULL);
	pHelpModule->fLastLoadingTime = time (NULL) + 1e7;  // pour ne pas qu'il soit desactive lors d'un reload general, car il n'est pas dans la liste des modules actifs du fichier de conf.
}


static gchar *cairo_dock_extract_default_module_name_from_path (gchar *cSoFilePath)
{
	gchar *ptr = g_strrstr (cSoFilePath, "/");
	if (ptr == NULL)
		ptr = cSoFilePath;
	else
		ptr ++;
	if (strncmp (ptr, "lib", 3) == 0)
		ptr += 3;

	if (strncmp (ptr, "cd-", 3) == 0)
		ptr += 3;
	else if (strncmp (ptr, "cd_", 3) == 0)
		ptr += 3;

	gchar *cModuleName = g_strdup (ptr);

	ptr = g_strrstr (cModuleName, ".so");
	if (ptr != NULL)
		*ptr = '\0';

	//ptr = cModuleName;
	//while ((ptr = g_strrstr (ptr, "-")) != NULL)
	//	*ptr = '_';

	return cModuleName;
}

gchar *cairo_dock_check_module_conf_file (CairoDockVisitCard *pVisitCard)
{
	if (pVisitCard->cConfFileName == NULL)
		return NULL;
	
	int r;
	gchar *cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, pVisitCard->cUserDataDir);
	if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
	{
		cd_message ("directory %s doesn't exist, it will be added.", cUserDataDirPath);
		
		gchar *command = g_strdup_printf ("mkdir -p %s", cUserDataDirPath);
		r = system (command);
		g_free (command);
	}
	
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, pVisitCard->cConfFileName);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		cd_message ("no conf file %s, we will take the default one", cConfFilePath);
		gchar *command = g_strdup_printf ("cp %s/%s %s", pVisitCard->cShareDataDir, pVisitCard->cConfFileName, cConfFilePath);
		r = system (command);
		g_free (command);
	}
	
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // la copie ne s'est pas bien passee.
	{
		cd_warning ("couldn't copy %s/%s in %s; check permissions and file's existence", pVisitCard->cShareDataDir, pVisitCard->cConfFileName, cUserDataDirPath);
		g_free (cUserDataDirPath);
		g_free (cConfFilePath);
		return NULL;
	}
	
	g_free (cUserDataDirPath);
	return cConfFilePath;
}

void cairo_dock_free_visit_card (CairoDockVisitCard *pVisitCard)
{
	g_free (pVisitCard);  // toutes les chaines sont statiques.
}

static void cairo_dock_open_module (CairoDockModule *pCairoDockModule, GError **erreur)
{
	//\__________________ On ouvre le .so.
	GModule *module = g_module_open (pCairoDockModule->cSoFilePath, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	if (!module)
	{
		g_set_error (erreur, 1, 1, "while opening module '%s' : (%s)", pCairoDockModule->cSoFilePath, g_module_error ());
		return ;
	}
	pCairoDockModule->pModule = module;

	//\__________________ On identifie le module.
	gboolean bSymbolFound;
	CairoDockModulePreInit function_pre_init = NULL;
	bSymbolFound = g_module_symbol (module, "pre_init", (gpointer) &function_pre_init);
	if (bSymbolFound && function_pre_init != NULL)
	{
		pCairoDockModule->pVisitCard = g_new0 (CairoDockVisitCard, 1);
		pCairoDockModule->pInterface = g_new0 (CairoDockModuleInterface, 1);
		gboolean bModuleLoaded = function_pre_init (pCairoDockModule->pVisitCard, pCairoDockModule->pInterface);
		if (! bModuleLoaded)
		{
			cairo_dock_free_visit_card (pCairoDockModule->pVisitCard);
			pCairoDockModule->pVisitCard = NULL;
			cd_debug ("module '%s' has not been loaded", pCairoDockModule->cSoFilePath);  // peut arriver a xxx-integration.
			return ;
		}
	}
	else
	{
		pCairoDockModule->pVisitCard = NULL;
		g_set_error (erreur, 1, 1, "this module ('%s') does not have the common entry point 'pre_init', it may be broken or icompatible with cairo-dock", pCairoDockModule->cSoFilePath);
		return ;
	}
	
	//\__________________ On verifie sa compatibilite.
	CairoDockVisitCard *pVisitCard = pCairoDockModule->pVisitCard;
	if (! g_bEasterEggs &&
		(pVisitCard->iMajorVersionNeeded > g_iMajorVersion || (pVisitCard->iMajorVersionNeeded == g_iMajorVersion && pVisitCard->iMinorVersionNeeded > g_iMinorVersion) || (pVisitCard->iMajorVersionNeeded == g_iMajorVersion && pVisitCard->iMinorVersionNeeded == g_iMinorVersion && pVisitCard->iMicroVersionNeeded > g_iMicroVersion)))
	{
		g_set_error (erreur, 1, 1, "this module ('%s') needs at least Cairo-Dock v%d.%d.%d, but Cairo-Dock is in v%d.%d.%d (%s)\n  It will be ignored", pCairoDockModule->cSoFilePath, pVisitCard->iMajorVersionNeeded, pVisitCard->iMinorVersionNeeded, pVisitCard->iMicroVersionNeeded, g_iMajorVersion, g_iMinorVersion, g_iMicroVersion, CAIRO_DOCK_VERSION);
		cairo_dock_free_visit_card (pCairoDockModule->pVisitCard);
		pCairoDockModule->pVisitCard = NULL;
		return ;
	}
	if (! g_bEasterEggs &&
		pVisitCard->cDockVersionOnCompilation != NULL && strcmp (pVisitCard->cDockVersionOnCompilation, CAIRO_DOCK_VERSION) != 0)  // separation des versions en easter egg.
	{
		g_set_error (erreur, 1, 1, "this module ('%s') was compiled with Cairo-Dock v%s, but Cairo-Dock is in v%s\n  It will be ignored", pCairoDockModule->cSoFilePath, pVisitCard->cDockVersionOnCompilation, CAIRO_DOCK_VERSION);
		cairo_dock_free_visit_card (pCairoDockModule->pVisitCard);
		pCairoDockModule->pVisitCard = NULL;
		return ;
	}

	if (pVisitCard->cModuleName == NULL)
		pVisitCard->cModuleName = cairo_dock_extract_default_module_name_from_path (pCairoDockModule->cSoFilePath);

	if (cairo_dock_module_is_auto_loaded (pCairoDockModule))  // c'est un module qui soit ne peut etre activer et/ou desactiver, soit s'est lie a un module interne; on l'activera donc automatiquement.
	{
		s_AutoLoadedModules = g_list_prepend (s_AutoLoadedModules, pCairoDockModule);
	}
}

static void cairo_dock_close_module (CairoDockModule *module)
{
	if (module->pModule)
		g_module_close (module->pModule);
	
	g_free (module->pInterface);
	module->pInterface = NULL;
	
	cairo_dock_free_visit_card (module->pVisitCard);
	module->pVisitCard = NULL;
	
	g_free (module->cConfFilePath);
	module->cConfFilePath = NULL;
}



CairoDockModule * cairo_dock_load_module (gchar *cSoFilePath, GError **erreur)  // cSoFilePath vers un fichier de la forme 'libtruc.so'. Le module est rajoute dans la table des modules.
{
	//g_print ("%s (%s)\n", __func__, cSoFilePath);
	if (cSoFilePath == NULL)  // g_module_open () plante si 'cSoFilePath' est NULL.
	{
		g_set_error (erreur, 1, 1, "%s () : no such module", __func__);
		return NULL;
	}

	CairoDockModule *pCairoDockModule = g_new0 (CairoDockModule, 1);
	pCairoDockModule->cSoFilePath = g_strdup (cSoFilePath);

	GError *tmp_erreur = NULL;
	cairo_dock_open_module (pCairoDockModule, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		g_free (pCairoDockModule);
		return NULL;
	}

	if (s_hModuleTable != NULL && pCairoDockModule->pVisitCard != NULL)
		g_hash_table_insert (s_hModuleTable, (gpointer)pCairoDockModule->pVisitCard->cModuleName, pCairoDockModule);

	return pCairoDockModule;
}

gboolean cairo_dock_register_module (CairoDockModule *pModule)
{
	g_return_val_if_fail (s_hModuleTable != NULL && pModule->pVisitCard != NULL && pModule->pVisitCard->cModuleName != NULL, FALSE);
	
	if (g_hash_table_lookup (s_hModuleTable, pModule->pVisitCard->cModuleName) != NULL)
	{
		cd_warning ("a module with the name '%s' is already registered", pModule->pVisitCard->cModuleName);
		return FALSE;
	}
	
	if (pModule->pVisitCard->cDockVersionOnCompilation == NULL)
		pModule->pVisitCard->cDockVersionOnCompilation = CAIRO_DOCK_VERSION;
	g_hash_table_insert (s_hModuleTable, (gpointer)pModule->pVisitCard->cModuleName, pModule);
	return TRUE;
}


void cairo_dock_preload_module_from_directory (const gchar *cModuleDirPath, GError **erreur)
{
	cd_message ("%s (%s)", __func__, cModuleDirPath);
	GError *tmp_erreur = NULL;
	GDir *dir = g_dir_open (cModuleDirPath, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return ;
	}

	CairoDockModule *pModule;
	const gchar *cFileName;
	GString *sFilePath = g_string_new ("");
	do
	{
		cFileName = g_dir_read_name (dir);
		if (cFileName == NULL)
			break ;
		
		if (g_str_has_suffix (cFileName, ".so"))
		{
			g_string_printf (sFilePath, "%s/%s", cModuleDirPath, cFileName);
			pModule = cairo_dock_load_module (sFilePath->str, &tmp_erreur);
			if (tmp_erreur != NULL)
			{
				cd_warning (tmp_erreur->message);
				g_error_free (tmp_erreur);
				tmp_erreur = NULL;
			}
		}
	}
	while (1);
	g_string_free (sFilePath, TRUE);
	g_dir_close (dir);
}



void cairo_dock_activate_modules_from_list (gchar **cActiveModuleList, double fTime)
{
	//\_______________ On active les modules auto-charges en premier.
	GError *erreur = NULL;
	gchar *cModuleName;
	CairoDockModule *pModule;
	GList *m;
	for (m = s_AutoLoadedModules; m != NULL; m = m->next)
	{
		pModule = m->data;
		pModule->fLastLoadingTime = fTime;
		if (pModule->pInstancesList == NULL)  // on ne les active qu'une seule fois. Si lors d'un changement de theme on re-active les modules, ceux-la resteront inchanges.
		{
			cairo_dock_activate_module (pModule, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
		}
	}
	
	if (cActiveModuleList == NULL)
		return ;
	
	//\_______________ On active tous les autres.
	int i;
	GList *pNotFoundModules = NULL;
	for (i = 0; cActiveModuleList[i] != NULL; i ++)
	{
		cModuleName = cActiveModuleList[i];
		pModule = g_hash_table_lookup (s_hModuleTable, cModuleName);
		if (pModule == NULL)
		{
			cd_debug ("No such module (%s)", cModuleName);
			pNotFoundModules = g_list_prepend (pNotFoundModules, cModuleName);
			continue ;
		}
		
		pModule->fLastLoadingTime = fTime;
		if (pModule->pInstancesList == NULL)
		{
			cairo_dock_activate_module (pModule, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
		}
		else
		{
			cairo_dock_reload_module (pModule, FALSE);
		}
	}
}

static void _cairo_dock_deactivate_one_old_module (gchar *cModuleName, CairoDockModule *pModule, double *fTime)
{
	if (pModule->fLastLoadingTime < *fTime)
		cairo_dock_deactivate_module (pModule);
}
void cairo_dock_deactivate_old_modules (double fTime)
{
	g_hash_table_foreach (s_hModuleTable, (GHFunc) _cairo_dock_deactivate_one_old_module, &fTime);
}



void cairo_dock_free_module (CairoDockModule *module)
{
	if (module == NULL)
		return ;
	cd_debug ("%s (%s)", __func__, module->pVisitCard->cModuleName);

	cairo_dock_deactivate_module (module);

	cairo_dock_close_module (module);

	cairo_dock_free_visit_card (module->pVisitCard);
	g_free (module->cSoFilePath);
	g_free (module);
}

void cairo_dock_unregister_module (const gchar *cModuleName)
{
	g_return_if_fail (cModuleName != NULL);
	g_hash_table_remove (s_hModuleTable, cModuleName);
}

GKeyFile *cairo_dock_pre_read_module_instance_config (CairoDockModuleInstance *pInstance, CairoDockMinimalAppletConfig *pMinimalConfig)
{
	g_return_val_if_fail (pInstance != NULL, NULL);
	if (pInstance->cConfFilePath == NULL)  // peut arriver avec xxx-integration par exemple.
		return NULL;
	gchar *cInstanceConfFilePath = pInstance->cConfFilePath;
	CairoDockModule *pModule = pInstance->pModule;
	
	GError *erreur = NULL;
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cInstanceConfFilePath);
	if (pKeyFile == NULL)
		return NULL;

	/**gboolean bNeedsUpgrade = cairo_dock_conf_file_needs_update (pKeyFile, pModule->pVisitCard->cModuleVersion);
	if (bNeedsUpgrade)
	{
		cairo_dock_flush_conf_file (pKeyFile, cInstanceConfFilePath, pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);
		g_key_file_free (pKeyFile);
		pKeyFile = cairo_dock_open_key_file (cInstanceConfFilePath);
		if (pKeyFile == NULL)
			return NULL;
	}*/

	if (! g_key_file_has_group (pKeyFile, "Icon"))  // ce module n'a pas d'icone, ce n'est donc pas une applet.
	{
		pMinimalConfig->iDesiredIconWidth = -1;
		pMinimalConfig->iDesiredIconHeight = -1;
		return pKeyFile;
	}
	
	gboolean bUseless;
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icon", "icon ", bUseless, pMinimalConfig->iDesiredIconWidth, pMinimalConfig->iDesiredIconHeight);
	
	if (pMinimalConfig->iDesiredIconWidth == 0/* || pMinimalConfig->iDesiredIconWidth > myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLET]*/)
		pMinimalConfig->iDesiredIconWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLET];
	if (pMinimalConfig->iDesiredIconHeight == 0/* || pMinimalConfig->iDesiredIconHeight > myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLET]*/)
		pMinimalConfig->iDesiredIconHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLET];
	pMinimalConfig->cLabel = cairo_dock_get_string_key_value (pKeyFile, "Icon", "name", NULL, NULL, NULL, NULL);
	pMinimalConfig->cIconFileName = cairo_dock_get_string_key_value (pKeyFile, "Icon", "icon", NULL, NULL, NULL, NULL);
	pMinimalConfig->fOrder = cairo_dock_get_double_key_value (pKeyFile, "Icon", "order", NULL, CAIRO_DOCK_LAST_ORDER, NULL, NULL);
	if (pMinimalConfig->fOrder == 0 || pMinimalConfig->fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		pMinimalConfig->fOrder = ++ s_iMaxOrder;
		g_key_file_set_double (pKeyFile, "Icon", "order", pMinimalConfig->fOrder);
		//g_print ("set order\n");
		cairo_dock_write_keys_to_file (pKeyFile, cInstanceConfFilePath);
	}
	else
	{
		s_iMaxOrder = MAX (s_iMaxOrder, pMinimalConfig->fOrder);
	}
	pMinimalConfig->cDockName = cairo_dock_get_string_key_value (pKeyFile, "Icon", "dock name", NULL, NULL, NULL, NULL);
	
	CairoDeskletAttribute *pDeskletAttribute = &pMinimalConfig->deskletAttribute;
	if (! g_key_file_has_group (pKeyFile, "Desklet"))  // cette applet ne peut pas se detacher.
	{
		pDeskletAttribute->iDeskletWidth = -1;
		pDeskletAttribute->iDeskletHeight = -1;
	}
	else
	{
		pMinimalConfig->bIsDetached = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "initially detached", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->bDeskletUseSize = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "use size", NULL, TRUE, NULL, NULL);
		
		//pDeskletAttribute->iDeskletWidth = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "width", NULL, 92, NULL, NULL);
		//pDeskletAttribute->iDeskletHeight = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "height", NULL, 92, NULL, NULL);
		cairo_dock_get_size_key_value_helper (pKeyFile, "Desklet", "", bUseless, pDeskletAttribute->iDeskletWidth, pDeskletAttribute->iDeskletHeight);
		//g_print ("desklet : %dx%d\n", pDeskletAttribute->iDeskletWidth, pDeskletAttribute->iDeskletHeight);
		if (pDeskletAttribute->iDeskletWidth == 0)
			pDeskletAttribute->iDeskletWidth = 96;
		if (pDeskletAttribute->iDeskletHeight == 0)
			pDeskletAttribute->iDeskletHeight = 96;
		
		pDeskletAttribute->iDeskletPositionX = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "x position", NULL, 0, NULL, NULL);
		pDeskletAttribute->iDeskletPositionY = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "y position", NULL, 0, NULL, NULL);
		/*pDeskletAttribute->bKeepBelow = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "keep below", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->bKeepAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "keep above", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->bOnWidgetLayer = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "on widget layer", NULL, FALSE, NULL, NULL);*/
		pDeskletAttribute->iAccessibility = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "accessibility", NULL, 0, NULL, NULL);
		pDeskletAttribute->bOnAllDesktops = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "sticky", NULL, TRUE, NULL, NULL);
		pDeskletAttribute->iNumDesktop = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "num desktop", NULL, -1, NULL, NULL);
		pDeskletAttribute->bPositionLocked = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "locked", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->bNoInput = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "no input", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->iRotation = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "rotation", NULL, 0, NULL, NULL);
		pDeskletAttribute->iDepthRotationY = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "depth rotation y", NULL, 0, NULL, NULL);
		pDeskletAttribute->iDepthRotationX = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "depth rotation x", NULL, 0, NULL, NULL);
		
		gchar *cDecorationTheme = cairo_dock_get_string_key_value (pKeyFile, "Desklet", "decorations", NULL, NULL, NULL, NULL);
		if (cDecorationTheme == NULL || strcmp (cDecorationTheme, "personnal") == 0)
		{
			//g_print ("on recupere les decorations personnelles au desklet\n");
			CairoDeskletDecoration *pUserDeskletDecorations = g_new0 (CairoDeskletDecoration, 1);
			pDeskletAttribute->pUserDecoration = pUserDeskletDecorations;
			
			pUserDeskletDecorations->cBackGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklet", "bg desklet", NULL, NULL, NULL, NULL);
			pUserDeskletDecorations->cForeGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklet", "fg desklet", NULL, NULL, NULL, NULL);
			pUserDeskletDecorations->iLoadingModifier = CAIRO_DOCK_FILL_SPACE;
			pUserDeskletDecorations->fBackGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "bg alpha", NULL, 1.0, NULL, NULL);
			pUserDeskletDecorations->fForeGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "fg alpha", NULL, 1.0, NULL, NULL);
			pUserDeskletDecorations->iLeftMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "left offset", NULL, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
			pUserDeskletDecorations->iTopMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "top offset", NULL, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
			pUserDeskletDecorations->iRightMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "right offset", NULL, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
			pUserDeskletDecorations->iBottomMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "bottom offset", NULL, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
			g_free (cDecorationTheme);
		}
		else
		{
			//g_print ("decorations : %s\n", cDecorationTheme);
			pDeskletAttribute->cDecorationTheme = cDecorationTheme;
		}
	}
	return pKeyFile;
}

void cairo_dock_free_minimal_config (CairoDockMinimalAppletConfig *pMinimalConfig)
{
	if (pMinimalConfig == NULL)
		return;
	g_free (pMinimalConfig->cLabel);
	g_free (pMinimalConfig->cIconFileName);
	g_free (pMinimalConfig->cDockName);
	g_free (pMinimalConfig->deskletAttribute.cDecorationTheme);
	cairo_dock_free_desklet_decoration (pMinimalConfig->deskletAttribute.pUserDecoration);
	g_free (pMinimalConfig);
}

void cairo_dock_activate_module (CairoDockModule *module, GError **erreur)
{
	g_return_if_fail (module != NULL);
	cd_message ("%s (%s)", __func__, module->pVisitCard->cModuleName);

	if (module->pInstancesList != NULL)
	{
		cd_warning ("module %s already activated", module->pVisitCard->cModuleName);
		g_set_error (erreur, 1, 1, "%s () : module %s is already active !", __func__, module->pVisitCard->cModuleName);
		return ;
	}

	g_free (module->cConfFilePath);
	module->cConfFilePath = cairo_dock_check_module_conf_file (module->pVisitCard);
	
	gchar *cInstanceFilePath = NULL;
	int j = 0;
	do
	{
		if (j == 0)
			cInstanceFilePath = g_strdup (module->cConfFilePath);
		else
			cInstanceFilePath = g_strdup_printf ("%s-%d",  module->cConfFilePath, j);
		
		if (cInstanceFilePath != NULL && ! g_file_test (cInstanceFilePath, G_FILE_TEST_EXISTS))  // la 1ere condition est pour xxx-integration par exemple .
		{
			g_free (cInstanceFilePath);
			break ;
		}
		
		cairo_dock_instanciate_module (module, cInstanceFilePath);  // prend possession de 'cInstanceFilePath'.
		
		j ++;
	} while (1);
	
	if (j == 0)
	{
		g_set_error (erreur, 1, 1, "%s () : no instance of module %s could be created", __func__, module->pVisitCard->cModuleName);
		return ;
	}
}


void cairo_dock_deactivate_module (CairoDockModule *module)
{
	g_return_if_fail (module != NULL);
	cd_debug ("%s (%s, %s)", __func__, module->pVisitCard->cModuleName, module->cConfFilePath);
	g_list_foreach (module->pInstancesList, (GFunc) cairo_dock_stop_module_instance, NULL);
	g_list_foreach (module->pInstancesList, (GFunc) cairo_dock_free_module_instance, NULL);
	g_list_free (module->pInstancesList);
	module->pInstancesList = NULL;
}


void cairo_dock_reload_module_instance (CairoDockModuleInstance *pInstance, gboolean bReloadAppletConf)
{
	g_return_if_fail (pInstance != NULL);
	CairoDockModule *module = pInstance->pModule;
	cd_message ("%s (%s, %d)", __func__, module->pVisitCard->cModuleName, bReloadAppletConf);
	
	GError *erreur = NULL;
	CairoContainer *pCurrentContainer = pInstance->pContainer;
	pInstance->pContainer = NULL;
	CairoDock *pCurrentDock = pInstance->pDock;
	pInstance->pDock = NULL;
	CairoDesklet *pCurrentDesklet = pInstance->pDesklet;
	pInstance->pDesklet = NULL;
	gchar *cOldDockName = NULL;
	
	CairoContainer *pNewContainer = NULL;
	CairoDock *pNewDock = NULL;
	CairoDesklet *pNewDesklet = NULL;
	
	//\______________ On recharge la config minimale.
	gboolean bModuleReloaded = FALSE;
	Icon *pIcon = pInstance->pIcon;
	GKeyFile *pKeyFile = NULL;
	CairoDockMinimalAppletConfig *pMinimalConfig = NULL;
	if (bReloadAppletConf && pInstance->cConfFilePath != NULL)
	{
		pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
		pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
		
		if (pMinimalConfig->iDesiredIconWidth > -1)  // c'est une applet.
		{
			//\______________ On met a jour les champs 'nom' et 'image' de l'icone.
			if (pIcon != NULL)
			{
				// on gere le changement de nom de son sous-dock.
				if (pIcon->cName != NULL && pIcon->pSubDock != NULL && cairo_dock_strings_differ (pIcon->cName, pMinimalConfig->cLabel))
				{
					gchar *cNewName = cairo_dock_get_unique_dock_name (pMinimalConfig->cLabel);
					cd_debug ("* le sous-dock %s prend le nom '%s'", pIcon->cName, cNewName);
					if (strcmp (pIcon->cName, cNewName) != 0)
						cairo_dock_rename_dock (pIcon->cName, NULL, cNewName);
					g_free (pMinimalConfig->cLabel);
					pMinimalConfig->cLabel = cNewName;
				}
				
				g_free (pIcon->cName);
				pIcon->cName = pMinimalConfig->cLabel;
				pMinimalConfig->cLabel = NULL;  // astuce.
				g_free (pIcon->cFileName);
				pIcon->cFileName = pMinimalConfig->cIconFileName;
				pMinimalConfig->cIconFileName = NULL;
			}
			
			// on recupere son dock (cree au besoin).
			if (!pMinimalConfig->bIsDetached)  // elle est desormais dans un dock.
			{
				const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
				pNewDock = cairo_dock_search_dock_from_name (cDockName);
				if (pNewDock == NULL)  // c'est un nouveau dock.
				{
					pNewDock = cairo_dock_create_new_dock (cDockName, NULL);
				}
				pNewContainer = CAIRO_CONTAINER (pNewDock);
			}
			
			// on la detache de son dock si son container a change.
			if (pCurrentDock != NULL && (pMinimalConfig->bIsDetached || pNewDock != pCurrentDock))
			{
				cd_message ("le container a change (%s -> %s)", pIcon->cParentDockName, pMinimalConfig->bIsDetached ? "desklet" : pMinimalConfig->cDockName);
				cOldDockName = g_strdup (pIcon->cParentDockName);
				cairo_dock_detach_icon_from_dock (pIcon, pCurrentDock, myIcons.iSeparateIcons);
			}
			
			// on recupere son desklet (cree au besoin).
			if (pMinimalConfig->bIsDetached)
			{
				if (pCurrentDesklet == NULL)  // c'est un nouveau desklet.
				{
					pNewDesklet = cairo_dock_create_desklet (pIcon, &pMinimalConfig->deskletAttribute);
				}
				else  // on reconfigure le desklet courant.
				{
					pNewDesklet = pCurrentDesklet;
					cairo_dock_configure_desklet (pNewDesklet, &pMinimalConfig->deskletAttribute);
				}
				pNewContainer = CAIRO_CONTAINER (pNewDesklet);
			}
		}
	}
	else
	{
		pNewContainer = pCurrentContainer;
		pNewDock = pCurrentDock;
		pNewDesklet = pCurrentDesklet;
	}
	pInstance->pContainer = pNewContainer;
	pInstance->pDock = pNewDock;
	pInstance->pDesklet = pNewDesklet;
	
	//\_______________________ On insere l'icone dans son nouveau dock, et on s'assure que sa taille respecte les tailles par defaut.
	if (pNewDock != NULL && pIcon != NULL)  // l'icone est desormais dans un dock.
	{
		// on recupere la taille voulue.
		if (pMinimalConfig == NULL)  // on recupere sa taille, car elle peut avoir change (si c'est la taille par defaut, ou si elle est devenue trop grande).
		{
			pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
			pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
			g_key_file_free (pKeyFile);
			pKeyFile = NULL;
		}
		pIcon->fWidth = pMinimalConfig->iDesiredIconWidth;
		pIcon->fHeight = pMinimalConfig->iDesiredIconHeight;
		
		// on charge l'icone a la bonne taille.
		cairo_dock_load_one_icon_from_scratch (pIcon, pNewContainer);
		
		// on insere l'icone dans le dock ou on met a jour celui-ci.
		if (pNewDock != pCurrentDock)  // on l'insere dans son nouveau dock.
		{
			cairo_dock_insert_icon_in_dock (pIcon, pNewDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
			pIcon->cParentDockName = g_strdup (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			cairo_dock_start_icon_animation (pIcon, pNewDock);
		}
		else  // le dock n'a pas change, on le met a jour.
		{
			pIcon->fWidth *= pNewContainer->fRatio;
			pIcon->fHeight *= pNewContainer->fRatio;
			
			if (bReloadAppletConf)
			{
				cairo_dock_update_dock_size (pNewDock);
				cairo_dock_calculate_dock_icons (pNewDock);
				gtk_widget_queue_draw (pNewContainer->pWidget);
			}
		}
	}
	
	//\_______________________ On recharge la config.
	if (pKeyFile != NULL)
	{
		cairo_dock_read_module_config (pKeyFile, pInstance);
	}
	gboolean bCanReload = TRUE;
	if (pInstance->pDrawContext != NULL)
	{
		cairo_destroy (pInstance->pDrawContext);
		//g_print ("on detruit le ctx de %s (%x)\n", pInstance->cConfFilePath, pInstance->pDrawContext);
	}
	if (pInstance->pDock)
	{
		//g_print ("dans un dock\n");
		if (pInstance->pIcon->pIconBuffer == NULL)
		{
			cd_warning ("invalid applet's icon buffer");
			pInstance->pDrawContext = NULL;
		}
		else
			pInstance->pDrawContext = cairo_create (pInstance->pIcon->pIconBuffer);
		if (cairo_status (pInstance->pDrawContext) != CAIRO_STATUS_SUCCESS)
		{
			cd_warning ("couldn't initialize drawing context, applet won't be reloaded !");
			bCanReload = FALSE;
		}
	}
	else
	{
		//g_print ("dans un desklet\n");
		pInstance->pDrawContext = NULL;
	}

	//\_______________________ On recharge l'instance.
	if (bCanReload && module->pInterface->reloadModule != NULL)
		bModuleReloaded = module->pInterface->reloadModule (pInstance, pCurrentContainer, pKeyFile);
	
	if (pNewContainer != pCurrentContainer && pNewDock != NULL && pCurrentDock != NULL && pIcon != NULL && pIcon->pSubDock != NULL)
	{
		cairo_dock_synchronize_one_sub_dock_position (pIcon->pSubDock, pNewDock, TRUE);
	}
	
	if (pNewDock != NULL && pNewDock->iRefCount != 0)  // on redessine l'icone pointant sur le sous-dock contenant l'applet, au cas ou son image aurait change.
	{
		cairo_dock_redraw_subdock_content (pNewDock);
	}
	
	if (pNewDock || pCurrentDock)
		cairo_dock_refresh_launcher_gui ();
	
	//\_______________________ On nettoie derriere nous.
	cairo_dock_free_minimal_config (pMinimalConfig);
	if (pKeyFile != NULL)
		g_key_file_free (pKeyFile);
	
	if (pCurrentDesklet != NULL && pCurrentDesklet != pNewDesklet)
		cairo_dock_free_desklet (pCurrentDesklet);
	if (pCurrentDock != NULL && pCurrentDock != pNewDock)
	{
		if (pCurrentDock->iRefCount == 0 && pCurrentDock->icons == NULL && !pCurrentDock->bIsMainDock)  // dock principal vide.
		{
			cairo_dock_destroy_dock (pCurrentDock, cOldDockName, NULL, NULL);
		}
		else
		{
			cairo_dock_update_dock_size (pCurrentDock);
			cairo_dock_calculate_dock_icons (pCurrentDock);
			gtk_widget_queue_draw (pCurrentContainer->pWidget);
		}
	}
	g_free (cOldDockName);
}

void cairo_dock_reload_module (CairoDockModule *pModule, gboolean bReloadAppletConf)
{
	GList *pElement;
	CairoDockModuleInstance *pInstance;
	for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	{
		pInstance = pElement->data;
		cairo_dock_reload_module_instance (pInstance, bReloadAppletConf);
	}
}


static void _cairo_dock_deactivate_one_module (gchar *cModuleName, CairoDockModule *pModule, gpointer data)
{
	cairo_dock_deactivate_module (pModule);
}
void cairo_dock_deactivate_all_modules (void)
{
	g_hash_table_foreach (s_hModuleTable, (GHFunc) _cairo_dock_deactivate_one_module, NULL);
}


void cairo_dock_activate_module_and_load (const gchar *cModuleName)
{
	if (g_pMainDock == NULL)
		return ;
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	pModule->fLastLoadingTime = 0;
	if (pModule->pInstancesList == NULL)
	{
		GError *erreur = NULL;
		cairo_dock_activate_module (pModule, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
		}
	}
	else
	{
		cairo_dock_reload_module (pModule, FALSE);
	}
	
	GList *pElement;
	CairoDockModuleInstance *pInstance;
	for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	{
		pInstance = pElement->data;
		if (pInstance->pDock)
		{
			cairo_dock_update_dock_size (pInstance->pDock);
			gtk_widget_queue_draw (pInstance->pDock->container.pWidget);
		}
	}
	
	cairo_dock_update_conf_file_with_active_modules ();
}

void cairo_dock_deactivate_module_instance_and_unload (CairoDockModuleInstance *pInstance)
{
	g_return_if_fail (pInstance != NULL);
	cd_message ("%s (%s)", __func__, pInstance->cConfFilePath);
	
	Icon *pIcon = pInstance->pIcon;  // l'instance va etre detruite.
	CairoDock *pDock = pInstance->pDock;
	if (pDock)
	{
		cairo_dock_remove_icon_from_dock (pDock, pInstance->pIcon);  // desinstancie le module et tout.
		cairo_dock_update_dock_size (pDock);
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
	else
	{
		cairo_dock_deinstanciate_module (pInstance);
		if (pIcon)
			pIcon->pModuleInstance = NULL;
	}
	cairo_dock_free_icon (pIcon);
}

void cairo_dock_deactivate_module_and_unload (const gchar *cModuleName)
{
	if (g_pMainDock == NULL)
		return ;
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	GList *pElement = pModule->pInstancesList, *pNextElement;
	CairoDockModuleInstance *pInstance;
	cd_debug ("%d instance(s) a arreter", g_list_length (pModule->pInstancesList));
	//for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	while (pElement != NULL)
	{
		pInstance = pElement->data;
		pNextElement = pElement->next;
		cairo_dock_deactivate_module_instance_and_unload (pInstance);
		pElement = pNextElement;
	}
	
	cairo_dock_update_conf_file_with_active_modules ();
}


CairoDockModule *cairo_dock_find_module_from_name (const gchar *cModuleName)
{
	//g_print ("%s (%s)\n", __func__, cModuleName);
	g_return_val_if_fail (cModuleName != NULL, NULL);
	return g_hash_table_lookup (s_hModuleTable, cModuleName);
}



static gboolean _cairo_dock_for_one_desklet (gchar *cModuleName, CairoDockModule *pModule, gpointer *data)
{
	GList *pElement;
	CairoDockModuleInstance *pInstance;
	for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	{
		pInstance = pElement->data;
		if (pInstance->pDesklet)
		{
			CairoDockForeachDeskletFunc pCallback = data[0];
			gpointer user_data = data[1];
			
			if (pCallback (pInstance->pDesklet, pInstance, user_data))
			{
				data[2] = pInstance;
				return TRUE;
			}
		}
		
	}
	
	return FALSE;
}
CairoDockModuleInstance *cairo_dock_foreach_desklet (CairoDockForeachDeskletFunc pCallback, gpointer user_data)
{
	gpointer data[3] = {pCallback, user_data, NULL};
	g_hash_table_find (s_hModuleTable, (GHRFunc) _cairo_dock_for_one_desklet, data);
	return data[2];
}

CairoDockModule *cairo_dock_foreach_module (GHRFunc pCallback, gpointer user_data)
{
	return g_hash_table_find (s_hModuleTable, (GHRFunc) pCallback, user_data);
}

static int _sort_module_by_alphabetical_order (CairoDockModule *m1, CairoDockModule *m2)
{
	if (!m1 || !m1->pVisitCard || !m1->pVisitCard->cModuleName)
		return 1;
	if (!m2 || !m2->pVisitCard || !m2->pVisitCard->cModuleName)
		return -1;
	return g_ascii_strncasecmp (dgettext (m1->pVisitCard->cGettextDomain, m1->pVisitCard->cModuleName),
		dgettext (m2->pVisitCard->cGettextDomain, m2->pVisitCard->cModuleName),
		-1);
}
CairoDockModule *cairo_dock_foreach_module_in_alphabetical_order (GCompareFunc pCallback, gpointer user_data)
{
	GList *pModuleList = g_hash_table_get_values (s_hModuleTable);
	pModuleList = g_list_sort (pModuleList, (GCompareFunc) _sort_module_by_alphabetical_order);
	
	CairoDockModule *pModule = (CairoDockModule *)g_list_find_custom (pModuleList, user_data, pCallback);
	
	g_list_free (pModuleList);
	return pModule;
}



static void _cairo_dock_write_one_module_name (gchar *cModuleName, CairoDockModule *pModule, GString *pString)
{
	if (pModule->pInstancesList != NULL && ! cairo_dock_module_is_auto_loaded (pModule) && pModule->cSoFilePath != NULL)
	{
		g_string_append_printf (pString, "%s;", cModuleName);
	}
}
gchar *cairo_dock_list_active_modules (void)
{
	GString *pString = g_string_new ("");
	
	g_hash_table_foreach (s_hModuleTable, (GHFunc) _cairo_dock_write_one_module_name, pString);
	
	if (pString->len > 0)
		pString->str[pString->len-1] = '\0';
	
	gchar *cModuleNames = pString->str;
	g_string_free (pString, FALSE);
	return cModuleNames;
}

void cairo_dock_update_conf_file_with_active_modules (void)
{
	gchar *cModuleNames = cairo_dock_list_active_modules ();
	
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_STRING, "System", "modules", cModuleNames,
		G_TYPE_INVALID);
	g_free (cModuleNames);
}


/*
* Cree une nouvelle instance d'un module. Cree l'icone et le container associe, et les place ou il faut.
*/
CairoDockModuleInstance *cairo_dock_instanciate_module (CairoDockModule *pModule, gchar *cConfFilePath)  // prend possession de 'cConfFilePah'.
{
	g_return_val_if_fail (pModule != NULL, NULL);
	cd_message ("%s (%s)", __func__, cConfFilePath);
	
	//\____________________ On cree une instance du module.
	//CairoDockModuleInstance *pInstance = g_new0 (CairoDockModuleInstance, 1);
	CairoDockModuleInstance *pInstance = calloc (1, sizeof (CairoDockModuleInstance) + pModule->pVisitCard->iSizeOfConfig + pModule->pVisitCard->iSizeOfData);
	pInstance->pModule = pModule;
	pInstance->cConfFilePath = cConfFilePath;
	/*if (pModule->pVisitCard->iSizeOfConfig > 0)
		pInstance->pConfig = g_new0 (gpointer, pModule->pVisitCard->iSizeOfConfig);
	if (pModule->pVisitCard->iSizeOfData > 0)
		pInstance->pData = g_new0 (gpointer, pModule->pVisitCard->iSizeOfData);*/
	
	CairoDockMinimalAppletConfig *pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
	GKeyFile *pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
	g_return_val_if_fail (cConfFilePath == NULL || pKeyFile != NULL, NULL);  // protection en cas de fichier de conf illisible.
	pModule->pInstancesList = g_list_prepend (pModule->pInstancesList, pInstance);
	
	//\____________________ On cree le container de l'instance, ainsi que son icone.
	CairoContainer *pContainer = NULL;
	CairoDock *pDock = NULL;
	CairoDesklet *pDesklet = NULL;
	Icon *pIcon = NULL;
	
	if (pMinimalConfig->iDesiredIconWidth > 0)  // le module a une icone, c'est donc une applet.
	{
		pInstance->bCanDetach = pMinimalConfig->deskletAttribute.iDeskletWidth > 0;
		pModule->bCanDetach = pInstance->bCanDetach;  // pas encore clair ...
		
		// on trouve/cree son container.
		if (pModule->bCanDetach && pMinimalConfig->bIsDetached)
		{
			pDesklet = cairo_dock_create_desklet (NULL, &pMinimalConfig->deskletAttribute);
			/*g_print ("transparence du desklet...\n");
			while (gtk_events_pending ())  // pour la transparence initiale.
				gtk_main_iteration ();*/
			pContainer = CAIRO_CONTAINER (pDesklet);
		}
		else
		{
			const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			pDock = cairo_dock_search_dock_from_name (cDockName);
			if (pDock == NULL)
			{
				pDock = cairo_dock_create_new_dock (cDockName, NULL);
			}
			pContainer = CAIRO_CONTAINER (pDock);
		}
		
		// on cree son icone.
		pIcon = cairo_dock_create_icon_for_applet (pMinimalConfig,
			pInstance,
			pContainer);
		if (pDesklet)
		{
			pDesklet->pIcon = pIcon;
			gtk_window_set_title (GTK_WINDOW(pContainer->pWidget), pInstance->pModule->pVisitCard->cModuleName);
			///gtk_widget_queue_draw (pContainer->pWidget);
		}
		cairo_dock_free_minimal_config (pMinimalConfig);
	}

	//\____________________ On initialise l'instance.
	if (pDock)
	{
		pIcon->fWidth *= pDock->container.fRatio;
		pIcon->fHeight *= pDock->container.fRatio;
	}
	
	pInstance->pIcon = pIcon;
	pInstance->pDock = pDock;
	pInstance->pDesklet = pDesklet;
	pInstance->pContainer = pContainer;
	
	if (pKeyFile)
		cairo_dock_read_module_config (pKeyFile, pInstance);
	
	gboolean bCanInit = TRUE;
	if (pDock)
	{
		if (pIcon != NULL)
		{
			if (pIcon->pIconBuffer == NULL)
			{
				cd_warning ("icon's buffer is NULL, applet won't be able to draw to it !");
				pInstance->pDrawContext = NULL;
			}
			else
				pInstance->pDrawContext = cairo_create (pIcon->pIconBuffer);
			if (cairo_status (pInstance->pDrawContext) != CAIRO_STATUS_SUCCESS)
			{
				cd_warning ("couldn't initialize drawing context, applet won't be able to draw itself !");
				bCanInit = FALSE;
			}
		}
	}
	else
		pInstance->pDrawContext = NULL;
	if (bCanInit && pModule->pInterface->initModule)
		pModule->pInterface->initModule (pInstance, pKeyFile);
	
	if (pDock)
	{
		pIcon->fWidth /= pDock->container.fRatio;
		pIcon->fHeight /= pDock->container.fRatio;
		cairo_dock_insert_icon_in_dock (pIcon, pDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
	}
	else if (pDesklet && pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0)  // peut arriver si le desklet a fini de se redimensionner avant l'init.
		gtk_widget_queue_draw (pDesklet->container.pWidget);
	if (pKeyFile != NULL)
		g_key_file_free (pKeyFile);
	return pInstance;
}

/*
* Detruit une instance de module et libere les resources associees.
*/
void cairo_dock_free_module_instance (CairoDockModuleInstance *pInstance)
{
	g_free (pInstance->cConfFilePath);
	/**g_free (pInstance->pConfig);
	g_free (pInstance->pData);*/
	g_free (pInstance);
}

/*
* Stoppe une instance d'un module en vue de la detruire.
*/
void cairo_dock_stop_module_instance (CairoDockModuleInstance *pInstance)
{
	if (pInstance->pModule->pInterface->stopModule != NULL)
		pInstance->pModule->pInterface->stopModule (pInstance);
	
	if (pInstance->pModule->pInterface->reset_data != NULL)
		pInstance->pModule->pInterface->reset_data (pInstance);
	
	if (pInstance->pModule->pInterface->reset_config != NULL)
		pInstance->pModule->pInterface->reset_config (pInstance);
	
	if (pInstance->pDesklet)
		cairo_dock_free_desklet (pInstance->pDesklet);
	if (pInstance->pDrawContext != NULL)
		cairo_destroy (pInstance->pDrawContext);
	
	if (pInstance->pIcon != NULL)
		pInstance->pIcon->pModuleInstance = NULL;
}
/*
* Stoppe une instance d'un module, et la detruit.
*/
void cairo_dock_deinstanciate_module (CairoDockModuleInstance *pInstance)
{
	cairo_dock_stop_module_instance (pInstance);
	
	pInstance->pModule->pInstancesList = g_list_remove (pInstance->pModule->pInstancesList, pInstance);
	
	cairo_dock_free_module_instance (pInstance);
}

/*
* Stoppe une instance d'un module, et la supprime.
*/
void cairo_dock_remove_module_instance (CairoDockModuleInstance *pInstance)
{
	cd_message ("%s (%s)", __func__, pInstance->cConfFilePath);
	//\_________________ Si c'est la derniere instance, on desactive le module.
	if (pInstance->pModule->pInstancesList->next == NULL)
	{
		cairo_dock_deactivate_module_and_unload (pInstance->pModule->pVisitCard->cModuleName);
		return ;
	}
	
	//\_________________ On efface le fichier de conf de cette instance.
	cd_debug ("on efface %s", pInstance->cConfFilePath);
	g_remove (pInstance->cConfFilePath);
	
	//\_________________ On supprime cette instance (on le fait maintenant, pour que son fichier de conf n'existe plus lors du 'stop'.
	gchar *cConfFilePath = pInstance->cConfFilePath;
	pInstance->cConfFilePath = NULL;
	CairoDockModule *pModule = pInstance->pModule;
	cairo_dock_deactivate_module_instance_and_unload (pInstance);  // pInstance n'est plus.
	
	//\_________________ Si c'est pas la derniere instance, la derniere instance prend sa place.
	int iNbInstances = g_list_length (pModule->pInstancesList)+1;  // nombre d'instances avant suppression.
	gchar *str = strrchr (cConfFilePath, '-');
	if (str == NULL || atoi (str+1) != iNbInstances-1)
	{
		gchar *cLastInstanceFilePath = g_strdup_printf ("%s-%d", pModule->cConfFilePath, iNbInstances-1);
		
		CairoDockModuleInstance *pOneInstance;
		GList *pElement;
		for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
		{
			pOneInstance = pElement->data;
			if (strcmp (pOneInstance->cConfFilePath, cLastInstanceFilePath) == 0)
			{
				gchar *cCommand = g_strdup_printf ("mv \"%s\" \"%s\"", cLastInstanceFilePath, cConfFilePath);
				int r = system (cCommand);
				g_free (cCommand);
				
				g_free (pOneInstance->cConfFilePath);
				pOneInstance->cConfFilePath = cConfFilePath;
				cConfFilePath = NULL;
				break ;
			}
		}
		
		g_free (cLastInstanceFilePath);
	}
	g_free (cConfFilePath);
}

void cairo_dock_add_module_instance (CairoDockModule *pModule)
{
	if (pModule->pInstancesList == NULL)
	{
		cd_warning ("This module has not been instanciated yet");
		return ;
	}
	int iNbInstances = g_list_length (pModule->pInstancesList);
	gchar *cInstanceFilePath = g_strdup_printf ("%s-%d", pModule->cConfFilePath, iNbInstances);
	if (! g_file_test (cInstanceFilePath, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("cp %s/%s %s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName, cInstanceFilePath);
		cd_debug (cCommand);
		int r = system (cCommand);
		g_free (cCommand);
	}
	
	CairoDockModuleInstance *pNewInstance = cairo_dock_instanciate_module (pModule, cInstanceFilePath);  // prend le 'cInstanceFilePath'.
	
	if (pNewInstance != NULL && pNewInstance->pDock)
	{
		cairo_dock_update_dock_size (pNewInstance->pDock);
	}
}

void cairo_dock_detach_module_instance (CairoDockModuleInstance *pInstance)
{
	gboolean bIsInDock = (pInstance->pDock != NULL);
	cairo_dock_update_conf_file (pInstance->cConfFilePath,
		G_TYPE_BOOLEAN, "Desklet", "initially detached", bIsInDock,
		G_TYPE_INVALID);
	cairo_dock_update_desklet_detached_state_in_gui (pInstance, bIsInDock);
	cairo_dock_reload_module_instance (pInstance, TRUE);
	if (pInstance->pDesklet)  // on a detache l'applet.
		cairo_dock_zoom_out_desklet (pInstance->pDesklet);
}


void cairo_dock_read_module_config (GKeyFile *pKeyFile, CairoDockModuleInstance *pInstance)
{
	CairoDockModuleInterface *pInterface = pInstance->pModule->pInterface;
	CairoDockVisitCard *pVisitCard = pInstance->pModule->pVisitCard;
	
	gboolean bFlushConfFileNeeded = FALSE;
	if (pInterface->read_conf_file != NULL)
	{	
		if (pInterface->reset_config != NULL)
			pInterface->reset_config (pInstance);
		if (pVisitCard->iSizeOfConfig != 0)
			memset (((gpointer)pInstance)+sizeof(CairoDockModuleInstance), 0, pVisitCard->iSizeOfConfig);
		
		bFlushConfFileNeeded = g_key_file_has_group (pKeyFile, "Desklet") && ! g_key_file_has_key (pKeyFile, "Desklet", "accessibility", NULL);  // petit hack des familles ^_^
		bFlushConfFileNeeded |= pInterface->read_conf_file (pInstance, pKeyFile);
	}
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, pVisitCard->cModuleVersion);
	if (bFlushConfFileNeeded)
		cairo_dock_flush_conf_file (pKeyFile, pInstance->cConfFilePath, pVisitCard->cShareDataDir, pVisitCard->cConfFileName);
}


static int s_iNbUsedSlots = 0;
static CairoDockModuleInstance *s_pUsedSlots[CAIRO_DOCK_NB_DATA_SLOT+1];
gboolean cairo_dock_reserve_data_slot (CairoDockModuleInstance *pInstance)
{
	g_return_val_if_fail (s_iNbUsedSlots < CAIRO_DOCK_NB_DATA_SLOT, FALSE);
	if (s_iNbUsedSlots == 0)
		memset (s_pUsedSlots, 0, (CAIRO_DOCK_NB_DATA_SLOT+1) * sizeof (CairoDockModuleInstance*));
	
	if (pInstance->iSlotID == 0)
	{
		s_iNbUsedSlots ++;
		if (s_pUsedSlots[s_iNbUsedSlots] == NULL)
		{
			pInstance->iSlotID = s_iNbUsedSlots;
			s_pUsedSlots[s_iNbUsedSlots] = pInstance;
		}
		else
		{
			int i;
			for (i = 1; i < s_iNbUsedSlots; i ++)
			{
				if (s_pUsedSlots[i] == NULL)
				{
					pInstance->iSlotID = i;
					s_pUsedSlots[i] = pInstance;
					break ;
				}
			}
		}
	}
	return TRUE;
}

void cairo_dock_release_data_slot (CairoDockModuleInstance *pInstance)
{
	if (pInstance->iSlotID == 0)
		return;
	s_iNbUsedSlots --;
	s_pUsedSlots[pInstance->iSlotID] = NULL;
	pInstance->iSlotID = 0;
}



#define REGISTER_INTERNAL_MODULE(cGroupName) \
	pModule = g_new0 (CairoDockInternalModule, 1);\
	cairo_dock_pre_init_##cGroupName (pModule);\
	g_hash_table_insert (pModuleTable, (gpointer)pModule->cModuleName, pModule)
void cairo_dock_preload_internal_modules (GHashTable *pModuleTable)
{
	cd_message ("");
	CairoDockInternalModule *pModule;
	
	REGISTER_INTERNAL_MODULE (Position);
	REGISTER_INTERNAL_MODULE (Accessibility);
	REGISTER_INTERNAL_MODULE (System);
	REGISTER_INTERNAL_MODULE (TaskBar);
	REGISTER_INTERNAL_MODULE (Background);
	REGISTER_INTERNAL_MODULE (Icons);
	REGISTER_INTERNAL_MODULE (Labels);
	REGISTER_INTERNAL_MODULE (Dialogs);
	REGISTER_INTERNAL_MODULE (Indicators);
	REGISTER_INTERNAL_MODULE (Views);
	REGISTER_INTERNAL_MODULE (Desklets);
}

void cairo_dock_reload_internal_module_from_keyfile (CairoDockInternalModule *pModule, GKeyFile *pKeyFile)
{
	gpointer *pPrevConfig = g_memdup (pModule->pConfig, pModule->iSizeOfConfig);
	memset (pModule->pConfig, 0, pModule->iSizeOfConfig);
	
	pModule->get_config (pKeyFile, pModule->pConfig);
	
	if (g_pMainDock != NULL)  // si on est en mode maintenance, inutile de recharger.
		pModule->reload (pPrevConfig, pModule->pConfig);
	
	if (pModule->reset_config)
		pModule->reset_config (pPrevConfig);
	g_free (pPrevConfig);
}

void cairo_dock_reload_internal_module (CairoDockInternalModule *pModule, const gchar *cConfFilePath)
{
	g_return_if_fail (pModule != NULL);
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return;
	
	cairo_dock_reload_internal_module_from_keyfile (pModule, pKeyFile);
	
	g_key_file_free (pKeyFile);
}

CairoDockInternalModule *cairo_dock_find_internal_module_from_name (const gchar *cModuleName)
{
	//g_print ("%s (%s)\n", __func__, cModuleName);
	g_return_val_if_fail (cModuleName != NULL, NULL);
	return g_hash_table_lookup (s_hInternalModuleTable, cModuleName);
}

gboolean cairo_dock_get_internal_module_config (CairoDockInternalModule *pModule, GKeyFile *pKeyFile)
{
	if (pModule->reset_config)
	{
		pModule->reset_config (pModule->pConfig);
	}
	memset (pModule->pConfig, 0, pModule->iSizeOfConfig);
	return pModule->get_config (pKeyFile, pModule->pConfig);
}

static void _cairo_dock_get_one_internal_module_config (gchar *cModuleName, CairoDockInternalModule *pModule, gpointer *data)
{
	GKeyFile *pKeyFile = data[0];
	gboolean *bFlushConfFileNeeded = data[1];
	*bFlushConfFileNeeded |= cairo_dock_get_internal_module_config (pModule, pKeyFile);
}
gboolean cairo_dock_get_global_config (GKeyFile *pKeyFile)
{
	gboolean bFlushConfFileNeeded = FALSE;
	gpointer data[2] = {pKeyFile, &bFlushConfFileNeeded};
	g_hash_table_foreach (s_hInternalModuleTable, (GHFunc) _cairo_dock_get_one_internal_module_config, data);
	return bFlushConfFileNeeded;
}


void cairo_dock_popup_module_instance_description (CairoDockModuleInstance *pModuleInstance)
{
	gchar *cDescription = g_strdup_printf ("%s (v%s) by %s\n%s",
		pModuleInstance->pModule->pVisitCard->cModuleName,
		pModuleInstance->pModule->pVisitCard->cModuleVersion,
		pModuleInstance->pModule->pVisitCard->cAuthor,
		dgettext (pModuleInstance->pModule->pVisitCard->cGettextDomain,
			pModuleInstance->pModule->pVisitCard->cDescription));
	
	cairo_dock_show_temporary_dialog_with_icon (cDescription, pModuleInstance->pIcon, pModuleInstance->pContainer, 0, pModuleInstance->pModule->pVisitCard->cIconFilePath);
	g_free (cDescription);
}


void cairo_dock_attach_to_another_module (CairoDockVisitCard *pVisitCard, const gchar *cOtherModuleName)
{
	CairoDockInternalModule *pInternalModule = cairo_dock_find_internal_module_from_name (cOtherModuleName);
	g_return_if_fail (pInternalModule != NULL && pInternalModule->iCategory == pVisitCard->iCategory && pVisitCard->cInternalModule == NULL);
	
	pInternalModule->pExternalModules = g_list_prepend (pInternalModule->pExternalModules, (gpointer)pVisitCard->cModuleName);
	pVisitCard->cInternalModule = cOtherModuleName;
}


int cairo_dock_get_nb_modules (void)
{
	return g_hash_table_size (s_hModuleTable);
}
