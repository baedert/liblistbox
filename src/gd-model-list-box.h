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
};

struct _GdModelListBoxClass
{
  GtkWidgetClass parent_class;
};

typedef struct _GdModelListBox GdModelListBox;

#define GD_TYPE_MODEL_LIST_BOX gd_model_list_box_get_type ()

G_DECLARE_FINAL_TYPE (GdModelListBox, gd_model_list_box, GD, MODEL_LIST_BOX, GtkWidget)

/*
 * TODO: Both the fill and the remove func (the latter nullable) should
 *       just be part of _set_model.
 */
GtkWidget *  gd_model_list_box_new             (void);
void         gd_model_list_box_set_model       (GdModelListBox *box,
                                                GListModel     *model);
GListModel * gd_model_list_box_get_model       (GdModelListBox *box);
void         gd_model_list_box_set_fill_func   (GdModelListBox           *self,
                                                GdModelListBoxFillFunc    func,
                                                gpointer                  user_data);
void         gd_model_list_box_set_remove_func (GdModelListBox           *self,
                                                GdModelListBoxRemoveFunc  func,
                                                gpointer                  user_data);
#endif
