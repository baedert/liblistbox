#ifndef _GD_MODEL_LIST_BOX_H_
#define _GD_MODEL_LIST_BOX_H_

#include <gtk/gtk.h>

typedef GtkWidget * (*GdModelListBoxFillFunc)   (gpointer  item,
                                                 GtkWidget *widget,
                                                 guint      item_index,
                                                 gpointer   user_data);
typedef void        (*GdModelListBoxRemoveFunc) (GtkWidget *widget,
                                                 gpointer   item,
                                                 gpointer   user_data);

struct _GdModelListBox
{
  GtkWidget parent_instance;

  GtkAdjustment *hadjustment;
  gulong vadjustment_value_changed_id;
  GtkAdjustment *vadjustment;

  GPtrArray *widgets;
  GPtrArray *pool;
  GdModelListBoxRemoveFunc remove_func;
  GdModelListBoxFillFunc fill_func;
  gpointer fill_func_data;
  gpointer remove_func_data;
  GListModel *model;

  guint model_from;
  guint model_to;
  double bin_y_diff;

  double last_value;

  GtkWidget *active_row;
};

struct _GdModelListBoxClass
{
  GtkWidgetClass parent_class;
};

typedef struct _GdModelListBox GdModelListBox;

#define GD_TYPE_MODEL_LIST_BOX gd_model_list_box_get_type ()

G_DECLARE_FINAL_TYPE (GdModelListBox, gd_model_list_box, GD, MODEL_LIST_BOX, GtkWidget)

GtkWidget *  gd_model_list_box_new             (void);
void         gd_model_list_box_set_model       (GdModelListBox           *box,
                                                GListModel               *model,
                                                GdModelListBoxFillFunc    fill_func,
                                                gpointer                  fill_data,
                                                GDestroyNotify            fill_destroy_notify,
                                                GdModelListBoxRemoveFunc  remove_func,
                                                gpointer                  remove_data,
                                                GDestroyNotify            remove_destroy_notify);
GListModel * gd_model_list_box_get_model       (GdModelListBox *box);

#endif
