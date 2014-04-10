/********************************************************************
 * dialog-column-view.c -- editor for simple column view of reports *
 * Copyright (C) 2001 Bill Gribble <grib@billgribble.com>           *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 ********************************************************************/

#include "config.h"

#include <gnome.h>
#include <guile/gh.h>
#include <g-wrap-wct.h>

#include "dialog-column-view.h"
#include "dialog-options.h"
#include "dialog-utils.h"
#include "messages.h"
#include "option-util.h"
#include "window-report.h"

struct gncp_column_view_edit {
  GNCOptionWin * optwin;
  GtkCList  * available;
  GtkCList  * contents; 

  SCM          options; 
  SCM          view;
  GNCOptionDB  * odb;

  SCM       available_list;
  int       available_selected;

  SCM       contents_list;
  int       contents_selected;
};


static void gnc_column_view_edit_add_cb(GtkButton * button,
                                        gpointer user_data);
static void gnc_column_view_edit_remove_cb(GtkButton * button,
                                           gpointer user_data);
static void gnc_edit_column_view_move_up_cb(GtkButton * button,
                                            gpointer user_data);
static void gnc_edit_column_view_move_down_cb(GtkButton * button,
                                              gpointer user_data);
static void gnc_column_view_edit_size_cb(GtkButton * button,
                                         gpointer user_data);


static void
gnc_column_view_set_option(GNCOptionDB * odb, char * section, char * name,
                           SCM new_value) {
  GNCOption *  option = 
    gnc_option_db_get_option_by_name(odb, section, name);
  
  if(option)
  {
    gnc_option_db_set_option(odb, section, name, new_value);

    /* set_option doesn't do this */
    gnc_option_set_changed (option, TRUE);
  }
}

static void 
gnc_column_view_edit_destroy(gnc_column_view_edit * view) {
  gnc_options_dialog_destroy(view->optwin);
  scm_unprotect_object(view->options);
  scm_unprotect_object(view->view);
  gnc_option_db_destroy(view->odb);
  g_free(view);
}

static void
update_display_lists(gnc_column_view_edit * view) {
  SCM   get_names = gh_eval_str("gnc:all-report-template-names");
  SCM   template_menu_name = gh_eval_str("gnc:report-template-menu-name/name");
  SCM   report_menu_name = gh_eval_str("gnc:report-menu-name");
  SCM   find_report = gh_eval_str("gnc:find-report");
  SCM   names = gh_call0(get_names);
  SCM   contents = 
    gnc_option_db_lookup_option(view->odb, "__general", "report-list",
                                SCM_BOOL_F);
  SCM   this_report, this_name;
  SCM   selection;
  char  * name[3];
  int   row, i;

  /* Update the list of available reports (left selection box). */
  row = view->available_selected;

  if(gh_list_p(view->available_list) && !gh_null_p (view->available_list)) {
    row = MIN (row, gh_length (view->available_list) - 1);
    selection = gh_list_ref (view->available_list, gh_int2scm (row));
  }
  else {
    selection = SCM_UNDEFINED;
  }

  scm_unprotect_object(view->available_list);
  view->available_list = names;
  scm_protect_object(view->available_list);

  gtk_clist_freeze(view->available);
  gtk_clist_clear(view->available);
  if(gh_list_p(names)) {
    for(i = 0; !gh_null_p(names); names = gh_cdr(names), i++) {
      if (gh_equal_p (gh_car(names), selection))
        row = i;
      name[0] = gh_scm2newstr(gh_call1(template_menu_name, gh_car(names)),
			      NULL);
      gtk_clist_append(view->available, name);
      free(name[0]);
    }
  }
  gtk_clist_select_row(view->available, row, 0);
  gtk_clist_thaw(view->available);


  /* Update the list of selected reports (right selection box). */
  row = view->contents_selected;

  if(gh_list_p(view->contents_list) && !gh_null_p (view->contents_list)) {
    row = MIN (row, gh_length (view->contents_list) - 1);
    selection = gh_list_ref (view->contents_list, gh_int2scm (row));
  }
  else {
    selection = SCM_UNDEFINED;
  }

  scm_unprotect_object(view->contents_list);
  view->contents_list = contents;
  scm_protect_object(view->contents_list);

  gtk_clist_freeze(view->contents);
  gtk_clist_clear(view->contents);
  if(gh_list_p(contents)) {
    for(i = 0; !gh_null_p(contents); contents = gh_cdr(contents), i++) {
      if (gh_equal_p (gh_car(contents), selection))
        row = i;

      this_report = gh_call1(find_report, gh_caar(contents));
      /* this_name = gh_call1(report_name, this_report); */
      this_name = gh_call1(report_menu_name, this_report);
      name[0] = gh_scm2newstr(this_name, NULL);
      name[1] = g_strdup_printf("%d", gh_scm2int(gh_cadr(gh_car(contents))));
      name[2] = g_strdup_printf("%d", gh_scm2int(gh_caddr(gh_car(contents))));
      gtk_clist_append(view->contents, name);
      free(name[0]);
      g_free(name[1]);
      g_free(name[2]);
    }
  }
  gtk_clist_select_row(view->contents, row, 0);
  gtk_clist_thaw(view->contents);

  gnc_clist_columns_autosize (view->available);
  gnc_clist_columns_autosize (view->contents);
}

static void
gnc_column_view_select_avail_cb(GtkCList * clist, gint row, gint col,
                                GdkEvent * ev, gpointer data) {
  gnc_column_view_edit * r = data;
  r->available_selected = row;  
}

static void
gnc_column_view_select_contents_cb(GtkCList * clist, gint row, gint col,
                                   GdkEvent * ev, gpointer data) {
  gnc_column_view_edit * r = data;
  r->contents_selected = row;  
}

static void
gnc_column_view_edit_apply_cb(GNCOptionWin * w, gpointer user_data) {
  SCM  dirty_report = gh_eval_str("gnc:report-set-dirty?!");
  gnc_column_view_edit * win = user_data;
  
  if(!win) return;
  gnc_option_db_commit(win->odb);
  gh_call2(dirty_report, win->view, SCM_BOOL_T);
}

static void
gnc_column_view_edit_close_cb(GNCOptionWin * win, gpointer user_data) {
  gnc_column_view_edit * r = user_data;
  SCM set_editor = gh_eval_str("gnc:report-set-editor-widget!");
  
  gh_call2(set_editor, r->view, SCM_BOOL_F);
  gnc_column_view_edit_destroy(r);
}


/********************************************************************
 * gnc_column_view_edit_options
 * create the editor. 
 ********************************************************************/

GtkWidget * 
gnc_column_view_edit_options(SCM options, SCM view) {
  SCM get_editor = gh_eval_str("gnc:report-editor-widget");
  SCM ptr;
  GtkWidget * editor;

  ptr = gh_call1(get_editor, view);
  if(ptr != SCM_BOOL_F) {
    GtkWindow * w = gw_wcp_get_ptr(ptr);
    gtk_window_present(w);
    return NULL;
  }
  else {
    gnc_column_view_edit * r = g_new0(gnc_column_view_edit, 1);
    GladeXML *xml;

    r->optwin = gnc_options_dialog_new(TRUE, NULL);

    xml = gnc_glade_xml_new ("report.glade", "view_contents_hbox");

    glade_xml_signal_connect_data
      (xml, "gnc_column_view_edit_add_cb",
       GTK_SIGNAL_FUNC (gnc_column_view_edit_add_cb), r);

    glade_xml_signal_connect_data
      (xml, "gnc_column_view_edit_remove_cb",
       GTK_SIGNAL_FUNC (gnc_column_view_edit_remove_cb), r);

    glade_xml_signal_connect_data
      (xml, "gnc_edit_column_view_move_up_cb",
       GTK_SIGNAL_FUNC (gnc_edit_column_view_move_up_cb), r);

    glade_xml_signal_connect_data
      (xml, "gnc_edit_column_view_move_down_cb",
       GTK_SIGNAL_FUNC (gnc_edit_column_view_move_down_cb), r);

    glade_xml_signal_connect_data
      (xml, "gnc_column_view_edit_size_cb",
       GTK_SIGNAL_FUNC (gnc_column_view_edit_size_cb), r);

    editor       = glade_xml_get_widget (xml, "view_contents_hbox");
    r->available = GTK_CLIST (glade_xml_get_widget (xml, "available_list"));
    r->contents  = GTK_CLIST (glade_xml_get_widget (xml, "contents_list"));
    r->options   = options;
    r->view      = view;
    r->available_selected = 0;
    r->available_list = SCM_EOL;
    r->contents_selected = 0;
    r->contents_list = SCM_EOL;
    r->odb       = gnc_option_db_new(r->options);

    gnc_build_options_dialog_contents(r->optwin, r->odb);

    gtk_notebook_append_page(GTK_NOTEBOOK(gnc_options_dialog_notebook
                                          (r->optwin)),
                             editor, 
                             gtk_label_new(_("Contents")));

    scm_protect_object(r->options);
    scm_protect_object(r->view);
    scm_protect_object(r->available_list);
    scm_protect_object(r->contents_list);

    gtk_signal_connect(GTK_OBJECT(r->available), "select_row", 
                       gnc_column_view_select_avail_cb, (gpointer)r);
    gtk_signal_connect(GTK_OBJECT(r->contents), "select_row", 
                       gnc_column_view_select_contents_cb, (gpointer)r);

    update_display_lists(r);

    gtk_clist_column_titles_passive (r->available);
    gtk_clist_column_titles_passive (r->contents);

    gnc_options_dialog_set_apply_cb(r->optwin, 
                                    gnc_column_view_edit_apply_cb, r);
    gnc_options_dialog_set_close_cb(r->optwin, 
                                    gnc_column_view_edit_close_cb, r);

    gtk_widget_show_all(gnc_options_dialog_widget(r->optwin));
    return gnc_options_dialog_widget(r->optwin);
  }
}

static void
gnc_column_view_edit_add_cb(GtkButton * button, gpointer user_data) {
  gnc_column_view_edit * r = user_data;
  SCM make_report = gh_eval_str("gnc:make-report");
  SCM mark_report = gh_eval_str("gnc:report-set-needs-save?!");
  SCM find_report = gh_eval_str("gnc:find-report");
  SCM template_name;
  SCM new_report;
  SCM newlist = SCM_EOL;
  SCM oldlist = r->contents_list;
  int count;
  int oldlength; 
  
  if(gh_list_p(r->available_list) && 
     (gh_length(r->available_list) > r->available_selected)) {
    template_name = gh_list_ref(r->available_list, 
                                gh_int2scm(r->available_selected));
    new_report = gh_call1(make_report, template_name);
    gh_call2(mark_report, gh_call1(find_report, new_report), SCM_BOOL_T);
    oldlength = gh_length(r->contents_list);
    
    if(oldlength > r->contents_selected) {
      for(count = 0; count < r->contents_selected; count++) {
        newlist = gh_cons(gh_car(oldlist), newlist);
        oldlist = gh_cdr(oldlist);
      }
      newlist = gh_append2(gh_reverse(gh_cons(SCM_LIST4(new_report,
                                                        gh_int2scm(1),
                                                        gh_int2scm(1),
                                                        SCM_BOOL_F), 
                                              newlist)),
                           oldlist);
    }
    else {
      newlist = gh_append2(oldlist, 
                           SCM_LIST1(SCM_LIST4(new_report,
                                               gh_int2scm(1),
                                               gh_int2scm(1),
                                               SCM_BOOL_F)));
      r->contents_selected = oldlength;
    }
    
    scm_unprotect_object(r->contents_list);
    r->contents_list = newlist;
    scm_protect_object(r->contents_list);
    
    gnc_column_view_set_option(r->odb, "__general", "report-list",
                               r->contents_list);
    gnc_options_dialog_changed (r->optwin);
  }

  update_display_lists(r);
}

static void
gnc_column_view_edit_remove_cb(GtkButton * button, gpointer user_data) {
  gnc_column_view_edit * r = user_data;
  SCM newlist = SCM_EOL;
  SCM oldlist = r->contents_list;
  int count;
  int oldlength;
  
  if(gh_list_p(r->contents_list)) {
    oldlength = gh_length(r->contents_list);
    if(oldlength > r->contents_selected) {
      for(count=0; count < r->contents_selected; count++) {
        newlist = gh_cons(gh_car(oldlist), newlist);
        oldlist = gh_cdr(oldlist);
      }
      if(count <= oldlength) {
        newlist = gh_append2(gh_reverse(newlist), gh_cdr(oldlist));
      }
    }
    
    if(oldlength == r->contents_selected + 1) {
      r->contents_selected --;
    }

    scm_unprotect_object(r->contents_list);
    r->contents_list = newlist;
    scm_protect_object(r->contents_list);

    gnc_column_view_set_option(r->odb, "__general", "report-list",
                               r->contents_list);

    gnc_options_dialog_changed (r->optwin);
  }

  update_display_lists(r);
}

static void
gnc_edit_column_view_move_up_cb(GtkButton * button, gpointer user_data) {
  gnc_column_view_edit * r = user_data;
  SCM oldlist = r->contents_list;
  SCM newlist = SCM_EOL;
  SCM temp;
  int oldlength;
  int count;

  oldlength = gh_length(r->contents_list);
  if((r->contents_selected > 0) && (oldlength > r->contents_selected)) {
    for(count = 1; count < r->contents_selected; count++) {
      newlist = gh_cons(gh_car(oldlist), newlist);
      oldlist = gh_cdr(oldlist);
    }
    temp = gh_car(oldlist);
    oldlist = gh_cdr(oldlist);
    newlist = gh_cons(temp, gh_cons(gh_car(oldlist), newlist));
    newlist = gh_append2(gh_reverse(newlist), gh_cdr(oldlist));

    scm_unprotect_object(r->contents_list);
    r->contents_list = newlist;
    scm_protect_object(r->contents_list);

    r->contents_selected = r->contents_selected - 1;

    gnc_column_view_set_option(r->odb, "__general", "report-list",
                               r->contents_list);
    
    gnc_options_dialog_changed (r->optwin);

    update_display_lists(r);
  }
}

static void
gnc_edit_column_view_move_down_cb(GtkButton * button, gpointer user_data) {
  gnc_column_view_edit * r = user_data;
  SCM oldlist = r->contents_list;
  SCM newlist = SCM_EOL;
  SCM temp;
  int oldlength;
  int count;

  oldlength = gh_length(r->contents_list);
  if(oldlength > (r->contents_selected + 1)) {
    for(count = 0; count < r->contents_selected; count++) {
      newlist = gh_cons(gh_car(oldlist), newlist);
      oldlist = gh_cdr(oldlist);
    }
    temp = gh_car(oldlist);
    oldlist = gh_cdr(oldlist);
    newlist = gh_cons(temp, gh_cons(gh_car(oldlist), newlist));
    newlist = gh_append2(gh_reverse(newlist), gh_cdr(oldlist));

    scm_unprotect_object(r->contents_list);
    r->contents_list = newlist;
    scm_protect_object(r->contents_list);

    r->contents_selected = r->contents_selected + 1;

    gnc_column_view_set_option(r->odb, "__general", "report-list",
                               r->contents_list);    

    gnc_options_dialog_changed (r->optwin);

    update_display_lists(r);
  }
}

static void
gnc_column_view_edit_size_cb(GtkButton * button, gpointer user_data) {
  gnc_column_view_edit * r = user_data;
  GtkWidget * rowspin;
  GtkWidget * colspin;
  GtkWidget * dlg;
  GladeXML *xml;
  SCM current;
  int length;
  int dlg_ret;

  xml = gnc_glade_xml_new ("report.glade", "Edit Report Size");
  dlg = glade_xml_get_widget (xml, "Edit Report Size");

  /* get the spinner widgets */ 
  rowspin = glade_xml_get_widget (xml, "row_spin");
  colspin = glade_xml_get_widget (xml, "col_spin");

  length = gh_length(r->contents_list);
  if(length > r->contents_selected) {
    current = gh_list_ref(r->contents_list, 
                          gh_int2scm(r->contents_selected));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(colspin),
                              (float)gh_scm2int(gh_cadr(current)));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rowspin),
                              (float)gh_scm2int(gh_caddr(current)));
  
    dlg_ret = gnome_dialog_run_and_close(GNOME_DIALOG(dlg));

    if(dlg_ret == 0) {
      current = SCM_LIST4(gh_car(current),
                          gh_int2scm(gtk_spin_button_get_value_as_int
                                     (GTK_SPIN_BUTTON(colspin))),
                          gh_int2scm(gtk_spin_button_get_value_as_int
                                     (GTK_SPIN_BUTTON(rowspin))),
                          SCM_BOOL_F);
      scm_unprotect_object(r->contents_list);
      r->contents_list = scm_list_set_x(r->contents_list, 
                                        gh_int2scm(r->contents_selected),
                                        current);
      scm_protect_object(r->contents_list);
      gnc_options_dialog_changed (r->optwin);
      update_display_lists(r);
    }
  }
}