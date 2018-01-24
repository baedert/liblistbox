#include <gtk/gtk.h>
#include <gd-model-list-box.h>

static const char *CSS =
"list > row {"
"  padding-left: 12px;"
"  padding-right: 12px;"
"  border-spacing: 12px;"
"}"
"list > row > image {"
"  background-color: black;"
"}"
;

#define ICON_SIZE 100


/* Test class {{{ */
struct _GdData
{
  GObject parent_instance;

  GCancellable *cancellable;
  char *image_path;
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
  GtkWidget *path_label;
};

typedef struct _GdRowWidget GdRowWidget;

G_DECLARE_FINAL_TYPE (GdRowWidget, gd_row_widget, GD, ROW_WIDGET, GtkBox)
G_DEFINE_TYPE (GdRowWidget, gd_row_widget, GTK_TYPE_BOX)
#define GD_TYPE_ROW_WIDGET gd_row_widget_get_type ()
#define GD_ROW_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GD_TYPE_ROW_WIDGET, GdRowWidget))

static GtkWidget * gd_row_widget_new (void) { return GTK_WIDGET (g_object_new (GD_TYPE_ROW_WIDGET, NULL)); }

static void gd_row_widget_init (GdRowWidget *d)
{
  d->image = gtk_image_new ();
  d->path_label = gtk_label_new ("");

  gtk_widget_set_size_request (d->image, ICON_SIZE, ICON_SIZE);
  gtk_style_context_add_class (gtk_widget_get_style_context (d->path_label), "dim-label");

  gtk_container_add (GTK_CONTAINER (d), d->image);
  gtk_container_add (GTK_CONTAINER (d), d->path_label);
}
static void gd_row_widget_class_init (GdRowWidgetClass *dc)
{
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (dc), "row");
}

// }}}

GListModel *model;

static void
remove_func (GtkWidget *widget,
             gpointer   item,
             gpointer   user_data)
{
  GdRowWidget *row = GD_ROW_WIDGET (widget);
  GdData *data = GD_DATA (item);

  // We remove the scaled image from memory, but leave the label.
  gtk_image_clear (GTK_IMAGE (row->image));
  if (data->cancellable) {
    g_cancellable_cancel (data->cancellable);
  }
  g_clear_object (&data->cancellable);
}

static void
icon_loaded_cb (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GdkPixbuf *icon;
  struct {
    GdData *item;
    GdRowWidget *row;
    GInputStream *stream;
  } *load_data = user_data;

  icon = gdk_pixbuf_new_from_stream_finish (result, NULL);
  // We might have gotten a cancelled error here!

  if (g_cancellable_is_cancelled (load_data->item->cancellable))
    goto cleanup;

  gtk_image_set_from_pixbuf (GTK_IMAGE (load_data->row->image), icon);

cleanup:
  g_clear_object (&load_data->item->cancellable);
  g_free (load_data);
  g_input_stream_close (load_data->stream, NULL, NULL);
}

static GtkWidget *
fill_func (gpointer   item,
           GtkWidget *old_widget,
           guint      item_index,
           gpointer   user_data)
{
  GdRowWidget *row;
  GdData *data = item;
  GFile *file;
  struct {
    GdData *item;
    GdRowWidget *row;
    GInputStream *stream;
  } *load_data;

  if (G_UNLIKELY (!old_widget))
    row = GD_ROW_WIDGET (gd_row_widget_new ());
  else
    row = GD_ROW_WIDGET (old_widget);

  gtk_label_set_label (GTK_LABEL (row->path_label), data->image_path);

  /// XXX LEAKAGE
  file = g_file_new_for_path (data->image_path);
  g_clear_object (&data->cancellable);
  data->cancellable = g_cancellable_new ();
  load_data = g_malloc (sizeof (*load_data));
  load_data->item = data;
  load_data->row = row;
  load_data->stream = (GInputStream *)g_file_read (file, NULL, NULL);
  gdk_pixbuf_new_from_stream_at_scale_async (load_data->stream,
                                             ICON_SIZE, ICON_SIZE, TRUE,
                                             data->cancellable,
                                             icon_loaded_cb,
                                             load_data);


  return GTK_WIDGET (row);
}

// Misc {{{
static gboolean
scroll_cb (GtkWidget     *widget,
           GdkFrameClock *frame_clock,
           gpointer       user_data)
{
  GtkAdjustment *a = user_data;
  const double increase = 50.0;
  double val = gtk_adjustment_get_value (a);
  double max_val;

  max_val = gtk_adjustment_get_upper (a) - gtk_adjustment_get_page_size (a);

  if (val + increase >= max_val)
    return G_SOURCE_REMOVE;

  gtk_adjustment_set_value (a, val + increase);

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

  gtk_adjustment_set_value (vadjustment,
                            gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment));
}

static void
to_top_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  GtkScrolledWindow *scroller = user_data;
  GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment (scroller);
  gtk_adjustment_set_value (vadjustment, 0);
}

// }}}

int
main (int argc, char **argv)
{
  gtk_init ();

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *list = gd_model_list_box_new ();
  GtkWidget *headerbar = gtk_header_bar_new ();
  GtkWidget *scroll_button = gtk_button_new_with_label ("Scroll");
  GtkWidget *to_bottom_button = gtk_button_new_with_label ("To Bottom");
  GtkWidget *to_top_button = gtk_button_new_with_label ("To Top");
  GtkCssProvider *css_provider;
  GFile *image_folder;
  const int N = 1000;

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider, CSS, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  // Load data
  model = (GListModel *)g_list_store_new (GD_TYPE_DATA);
  image_folder = g_file_new_for_path ("/usr/share/backgrounds/gnome");
  GFileEnumerator *enumerator = g_file_enumerate_children (image_folder, "standard::name,standard::display-name", 0, NULL, NULL);
  GFileInfo *child = NULL;
  while ((child = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
    {
      if (g_str_has_suffix (g_file_info_get_display_name (child), ".jpg"))
        {
          GFile *child_file = g_file_get_child (image_folder, g_file_info_get_name (child));
          GdData *item = g_object_new (GD_TYPE_DATA, NULL);
          item->image_path = g_file_get_path (child_file);

          g_list_store_append (G_LIST_STORE (model), item);
        }
    }

  // We duplicate all items once we have them in memory...
  const int k = g_list_model_get_n_items (model);
  for (int m = 0; m < N; m ++)
    {
      for (int i = 0; i < k; i ++)
        {
          GdData *new_item = g_object_new (GD_TYPE_DATA, NULL);
          GdData *old_item = (GdData *)g_list_model_get_object (model, i);

          new_item->image_path = g_strdup (old_item->image_path);

          g_list_store_append (G_LIST_STORE (model), new_item);

          g_object_unref (old_item);
        }
    }

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scroller), FALSE);


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

  char *title = g_strdup_printf ("%u items", g_list_model_get_n_items (model));
  gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), title);
  g_free (title);

  g_signal_connect (G_OBJECT (window), "close-request", G_CALLBACK (gtk_main_quit), NULL);

  gtk_window_resize (GTK_WINDOW (window), 700, 400);
  gtk_widget_show (window);
  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
