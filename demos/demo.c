#include <gtk/gtk.h>
#include "gd-model-list-box.h"
/*#include "profiling.h"*/

/* Test class {{{ */
struct _GdData
{
  GObject parent_instance;

  guint model_size;
  const gchar *text;
  guint on : 1;
  guint item_index;
};

typedef struct _GdData GdData;

G_DECLARE_FINAL_TYPE (GdData, gd_data, GD, DATA, GObject)
G_DEFINE_TYPE (GdData, gd_data, G_TYPE_OBJECT)
#define GD_TYPE_DATA gd_data_get_type ()

static void gd_data_init (GdData *d) {}
static void gd_data_class_init (GdDataClass *dc) {}
/* }}} */

/* Test Row widget {{{ */
struct _GdRowWidget
{
  GtkBox parent_instance;

  GtkWidget *image;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *button;
  GtkWidget *_switch;
  GtkWidget *entry;
  GtkWidget *scale;
  GtkWidget *stack;
  GtkWidget *remove_button;
};

typedef struct _GdRowWidget GdRowWidget;

G_DECLARE_FINAL_TYPE (GdRowWidget, gd_row_widget, GD, ROW_WIDGET, GtkBox)
G_DEFINE_TYPE (GdRowWidget, gd_row_widget, GTK_TYPE_BOX)
#define GD_TYPE_ROW_WIDGET gd_row_widget_get_type ()
#define GD_ROW_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GD_TYPE_ROW_WIDGET, GdRowWidget))

static GtkWidget * gd_row_widget_new (void) { return GTK_WIDGET (g_object_new (GD_TYPE_ROW_WIDGET, NULL)); }

static void gd_row_widget_init (GdRowWidget *d)
{
  gtk_box_set_spacing (GTK_BOX (d), 6);
  gtk_widget_set_margin_start (GTK_WIDGET (d), 12);
  gtk_widget_set_margin_end (GTK_WIDGET (d), 12);

  d->image = gtk_image_new ();
  d->label1 = gtk_label_new ("");
  d->label2 = gtk_label_new ("");
  d->button = gtk_button_new_with_label ("Click Me");
  d->_switch = gtk_switch_new ();
  d->entry = gtk_entry_new ();
  d->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  d->stack = gtk_stack_new ();
  d->remove_button = gtk_button_new_from_icon_name ("list-remove-symbolic");

  /*gtk_label_set_line_wrap (GTK_LABEL (d->label2), TRUE);*/
  gtk_label_set_ellipsize (GTK_LABEL (d->label2), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (d->label2, TRUE);
  gtk_widget_set_halign (d->label2, GTK_ALIGN_START);
  gtk_label_set_xalign (GTK_LABEL (d->label2), 0.0);
  gtk_widget_set_valign (d->_switch, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (d->_switch, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (d->entry, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (d->remove_button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (d->remove_button), "circular");



  gtk_container_add (GTK_CONTAINER (d->stack), d->_switch);
  gtk_container_add (GTK_CONTAINER (d->stack), d->scale);
  gtk_container_add (GTK_CONTAINER (d->stack), d->entry);

  gtk_container_add (GTK_CONTAINER (d), d->image);
  gtk_container_add (GTK_CONTAINER (d), d->label1);
  gtk_container_add (GTK_CONTAINER (d), d->label2);
  gtk_container_add (GTK_CONTAINER (d), d->button);
  gtk_container_add (GTK_CONTAINER (d), gtk_button_new_with_label ("Foo"));
  gtk_container_add (GTK_CONTAINER (d), d->stack);
  gtk_container_add (GTK_CONTAINER (d), d->remove_button);
}
static void gd_row_widget_class_init (GdRowWidgetClass *dc)
{
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (dc), "row");
}
/* }}} */


GtkSizeGroup *size_group1;
GtkSizeGroup *size_group2;

GListModel *model;

const guint N = 600000;

static const char *CSS =
"row:hover {"
"  background-color: alpha(grey, 0.2);"
"}"
"row:active {"
"  background-color: green;"
"}"
;

static void
switch_activated_cb (GtkSwitch *sw, GParamSpec *spec, gpointer user_data)
{
  GdData *data = user_data;

  data->on = gtk_switch_get_active (sw);
}

static void
remove_button_clicked_cb (GtkButton *source, gpointer user_data)
{
  guint item_index = GPOINTER_TO_UINT (user_data);

  g_debug ("#####################################################");

  g_debug ("Removing item at position %u", item_index);

  g_list_store_remove (G_LIST_STORE (model), item_index);
}

void
remove_func (GtkWidget *widget, gpointer item, gpointer user_data)
{
  GdRowWidget *row = GD_ROW_WIDGET (widget);

  g_signal_handlers_disconnect_matched (row->_switch,
                                        G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                        switch_activated_cb, NULL);
  g_signal_handlers_disconnect_matched (row->remove_button,
                                        G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                        remove_button_clicked_cb, NULL);
}

GtkWidget *
fill_func (gpointer   item,
           GtkWidget *old_widget,
           guint      item_index,
           gpointer   user_data)
{
  GdRowWidget *row;
  GdData *data = item;
  gchar *label;

  if (G_UNLIKELY (!old_widget))
    {
      row = GD_ROW_WIDGET (gd_row_widget_new ());
      gtk_size_group_add_widget (size_group1, row->label1);
      gtk_size_group_add_widget (size_group2, row->label2);
    }
  else
    {
      row = GD_ROW_WIDGET (old_widget);
    }

#if 0
  if (item_index < N / 2)
  /*if (item_index == 0)*/
    gtk_widget_set_size_request (GTK_WIDGET (row), -1, 100);
  else
    gtk_widget_set_size_request (GTK_WIDGET (row), -1, 400);
#else
  gtk_widget_set_size_request (GTK_WIDGET (row), -1, 100);
#endif

  gtk_image_set_from_icon_name (GTK_IMAGE (row->image), "list-add-symbolic");
  label = g_strdup_printf ("Row %'u of %'u", data->item_index + 1, data->model_size);
  gtk_label_set_label (GTK_LABEL (row->label1), label);
  gtk_label_set_markup (GTK_LABEL (row->label2), data->text);
  gtk_switch_set_active (GTK_SWITCH (row->_switch), data->on);
  /*gtk_widget_set_margin_top (GTK_WIDGET (row), MIN (200, item_index * 4));*/

  g_signal_connect (G_OBJECT (row->_switch), "notify::active", G_CALLBACK (switch_activated_cb), item);
  g_signal_connect (G_OBJECT (row->remove_button), "clicked",
                    G_CALLBACK (remove_button_clicked_cb), GUINT_TO_POINTER (item_index));

  g_free (label);

  return GTK_WIDGET (row);
}

static void
set_focus_cb (GtkWindow *window,
              GtkWidget *widget,
              gpointer   user_data)
{
  if (widget != NULL)
    g_debug ("New focus widget: %s %p", gtk_widget_get_name (widget), widget);
  else
    g_debug ("New focus widget: (NULL) NULL");
}

static gboolean
scroll_cb (GtkWidget     *widget,
           GdkFrameClock *frame_clock,
           gpointer       user_data)
{
  GtkAdjustment *a = user_data;
  double val = gtk_adjustment_get_value (a);
  double max_val;

  max_val = gtk_adjustment_get_upper (a) - gtk_adjustment_get_page_size (a);

  if (val + 1.0 >= max_val)
    return G_SOURCE_REMOVE;

  gtk_adjustment_set_value (a, val + 10.0);
  /*gtk_adjustment_set_value (a, val + 20.0);*/

  return G_SOURCE_CONTINUE;
}

static void
scroll_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  GtkScrolledWindow *scroller = user_data;
  GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment (scroller);

  gtk_widget_add_tick_callback (GTK_WIDGET (scroller),
                                scroll_cb,
                                vadjustment,
                                NULL);
}

static void
to_bottom_button_clicked_cb (GtkButton *button,
                             gpointer   user_data)
{
  GtkScrolledWindow *scroller = user_data;
  GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment (scroller);

  g_debug ("######### DOWN CLICKED ########");
  /* Obvious way of scrolling to the very bottom of a GtkAdjustment.
   * This case is interesting because doing so might change the upper
   * of the adjustment and the listbox has to compensate for that. */
  gtk_adjustment_set_value (vadjustment,
                            gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment));
}

static void
to_top_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  GtkScrolledWindow *scroller = user_data;
  GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment (scroller);

  g_debug ("################################### UP CLICKED");
  gtk_adjustment_set_value (vadjustment, 0);
}

int
main (int argc, char **argv)
{
  guint i;
  gtk_init ();

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *list = gd_model_list_box_new ();
  GtkWidget *headerbar = gtk_header_bar_new ();
  GtkWidget *scroll_button = gtk_button_new_with_label ("Scroll");
  GtkWidget *to_bottom_button = gtk_button_new_with_label ("To Bottom");
  GtkWidget *to_top_button = gtk_button_new_with_label ("To Top");
  GtkCssProvider *css_provider;

  g_signal_connect (window, "set-focus", G_CALLBACK (set_focus_cb), NULL);

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider, CSS, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scroller), FALSE);

  size_group1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  size_group2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  model = (GListModel *)g_list_store_new (GD_TYPE_DATA);
  for (i = 0; i < N; i ++)
    {
      GdData *d = g_object_new (GD_TYPE_DATA, NULL);
      d->item_index = i;
      d->model_size = N;
      d->text = "fpoobar'lsfasdf asdf <a href=\"foobar\">BLA BLA</a>asdfas df asd fasd f asdf as dfewrthuier htuiheasruig hdrhfughseduig hisdfiugsdhiugis<a href=\"foobar2\">BLA BLA 2</a>df";
      d->on = FALSE;
      g_list_store_append (G_LIST_STORE (model), d);
    }

  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (list), model,
                               fill_func, NULL, NULL,
                               remove_func, NULL, NULL);

  gtk_container_add (GTK_CONTAINER (scroller), list);
  gtk_container_add (GTK_CONTAINER (window), scroller);

  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (headerbar), TRUE);
  gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);
  g_signal_connect (scroll_button, "clicked", G_CALLBACK (scroll_button_clicked_cb), scroller);
  gtk_container_add (GTK_CONTAINER (headerbar), scroll_button);
  g_signal_connect (to_bottom_button, "clicked", G_CALLBACK (to_bottom_button_clicked_cb), scroller);
  gtk_container_add (GTK_CONTAINER (headerbar), to_bottom_button);
  g_signal_connect (to_top_button, "clicked", G_CALLBACK (to_top_button_clicked_cb), scroller);
  gtk_container_add (GTK_CONTAINER (headerbar), to_top_button);


  g_signal_connect (G_OBJECT (window), "close-request", G_CALLBACK (gtk_main_quit), NULL);

  gtk_window_resize (GTK_WINDOW (window), 700, 400);
  gtk_widget_show (window);
  gtk_main ();

  /*gtk_window_destroy (GTK_WINDOW (window));*/
  gtk_widget_destroy (window);

  return 0;
}
