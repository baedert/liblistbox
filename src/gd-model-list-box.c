/*
 *  Copyright 2017 Timm Bäder
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gd-model-list-box.h"

G_DEFINE_TYPE_WITH_CODE (GdModelListBox, gd_model_list_box, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));

#define Foreach_Row {guint i; for (i = 0; i < self->widgets->len; i ++){ \
                       GtkWidget *row = g_ptr_array_index (self->widgets, i);


enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

static GtkWidget *
get_widget (GdModelListBox *self,
            guint           index)
{
  gpointer item;
  GtkWidget *old_widget = NULL;
  GtkWidget *new_widget;

  item = g_list_model_get_item (self->model, index);

  if (self->pool->len > 0)
    old_widget = g_ptr_array_remove_index_fast (self->pool,
                                                self->pool->len - 1);

  new_widget = self->fill_func (item, old_widget, index, self->fill_func_data);
  g_assert (new_widget != NULL);
  g_assert (GTK_IS_WIDGET (new_widget));

  if (old_widget != NULL)
    g_assert (old_widget == new_widget);

  if (g_object_is_floating (new_widget))
    g_object_ref_sink (new_widget);

  return new_widget;
}

static void
insert_child_internal (GdModelListBox *self,
                       GtkWidget      *widget,
                       guint           index)
{
  if (gtk_widget_get_parent (widget) == NULL)
    gtk_widget_set_parent (widget, GTK_WIDGET (self));

  gtk_widget_set_child_visible (widget, TRUE);
  g_ptr_array_insert (self->widgets, index, widget);
}

static void
remove_child_by_index (GdModelListBox *self,
                       guint           index)
{
  GtkWidget *row;

  row = g_ptr_array_index (self->widgets, index);

  gtk_widget_set_child_visible (g_ptr_array_index (self->widgets, index), FALSE);

  if (self->remove_func)
    {
      guint item_index = self->model_from + index;
      self->remove_func (row,
                         g_list_model_get_item (self->model, item_index),
                         self->remove_func_data);
    }

  /* Can't use _fast for self->widgets, we need to keep the order. */
  g_ptr_array_remove_index (self->widgets, index);
  g_ptr_array_add (self->pool, row);
}

static inline int
requested_row_height (GdModelListBox *box,
                      GtkWidget      *w)
{
  int min;
  gtk_widget_measure (w,
                      GTK_ORIENTATION_VERTICAL,
                      gtk_widget_get_allocated_width (GTK_WIDGET (box)),
                      &min, NULL, NULL, NULL);
  return min;
}

// TODO(Perf): This function is useless as we always use it in a loop in ensure_visible_widgets
static inline int
row_y (GdModelListBox *self,
       guint           index)
{
  int y = 0;
  guint i;

  for (i = 0; i < index; i ++)
    y += requested_row_height (self, g_ptr_array_index (self->widgets, i));

  return y;
}

static int
estimated_row_height (GdModelListBox *self)
{
  int avg_widget_height = 0;

  Foreach_Row
    avg_widget_height += requested_row_height (self, row);
  }}

  if (self->widgets->len > 0)
    avg_widget_height /= self->widgets->len;
  else
    return 0;

  return avg_widget_height;

}

static inline int
bin_y (GdModelListBox *self)
{
  return - gtk_adjustment_get_value (self->vadjustment) + self->bin_y_diff;
}

static inline int
bin_height (GdModelListBox *self)
{
  int height = 0;
  int min;

  /* XXX This is only true if we actually allocate all rows at minimum height... */
  Foreach_Row
    gtk_widget_measure (row,
                        GTK_ORIENTATION_VERTICAL,
                        gtk_widget_get_allocated_width (GTK_WIDGET (self)),
                        &min, NULL, NULL, NULL);

    height += min;
  }}

  return height;
}

static int
estimated_list_height (GdModelListBox *self)
{
  int avg_height;
  int top_widgets;
  int bottom_widgets;
  int exact_height = 0;

  avg_height = estimated_row_height (self);
  top_widgets = self->model_from;
  bottom_widgets = g_list_model_get_n_items (self->model) - self->model_to;

  g_assert (top_widgets + bottom_widgets + self->widgets->len == g_list_model_get_n_items (self->model));

  Foreach_Row
    exact_height += requested_row_height (self, row);
  }}

  g_assert (self->widgets->len == (self->model_to - self->model_from));

  return (top_widgets * avg_height) +
         exact_height +
         (bottom_widgets * avg_height);
}

/**
 * When we set the vadjustment value from within this widget, we need to care about two things:
 *   
 *   1) Block the signal handler. It's mostly harmless but we don't want to unnecessarily
 *      redo things and we especially don't want to call queue_allocate or even queue_resize
 *      during size-allocate.
 *
 *   2) self->bin_y_diff depends on the adjustment value, so if we manually change the adjustment
 *      value, we also need to update self->bin_y_diff. This should happen in such a way that
 *      bin_y (self) returns the same value before and after setting the adjustment value
 *      and bin_y_diff.
 */
static void
set_vadjustment_value (GdModelListBox *self,
                       double          new_value)
{
  int old_bin_y = bin_y (self);
  double cur_value = gtk_adjustment_get_value (self->vadjustment);

  g_debug ("%s: Adjusting value from %f to %f", __FUNCTION__, cur_value, new_value);
  g_signal_handler_block (self->vadjustment,
                          self->vadjustment_value_changed_id);
  gtk_adjustment_set_value (self->vadjustment, new_value);
  g_signal_handler_unblock (self->vadjustment,
                            self->vadjustment_value_changed_id);
  g_assert_cmpint ((int)new_value, ==, (int)gtk_adjustment_get_value (self->vadjustment));
  g_debug ("bin_y_diff: %f, cur_value: %f, new_value: %f",
             self->bin_y_diff, cur_value, new_value);
  self->bin_y_diff -= (cur_value - new_value);
  g_assert_cmpint (bin_y (self), ==, old_bin_y);
}

static void
configure_adjustment (GdModelListBox *self)
{
  int widget_height;
  int list_height;
  int max_value;
  double cur_upper;
  double page_size;
  double cur_value;

  widget_height = gtk_widget_get_allocated_height (GTK_WIDGET (self));
  list_height   = estimated_list_height (self);
  cur_upper     = gtk_adjustment_get_upper (self->vadjustment);
  cur_value     = gtk_adjustment_get_value (self->vadjustment);
  page_size     = gtk_adjustment_get_page_size (self->vadjustment);

  if ((int)cur_upper != list_height)
    {
      gtk_adjustment_set_upper (self->vadjustment, list_height);
      g_debug ("Changing upper from %f to %d", cur_upper, list_height);
    }
  else if (list_height == 0)
    {
      gtk_adjustment_set_upper (self->vadjustment, widget_height);
    }

  if ((int)page_size != widget_height)
    gtk_adjustment_set_page_size (self->vadjustment, widget_height);

  gtk_adjustment_set_lower (self->vadjustment, 0.0);

  max_value = MAX (0, list_height - widget_height);
  if (cur_value > max_value)
    {
      g_debug ("2 ###############################################################");
      set_vadjustment_value (self, max_value);
      g_debug ("2 ###############################################################");
    }
}

static void
ensure_visible_widgets (GdModelListBox *self)
{
  int widget_height;
  int bottom_removed = 0;
  int bottom_added = 0;
  int top_removed = 0;
  int top_added = 0;

  g_debug (__FUNCTION__);

  if (!self->vadjustment ||
      !self->model ||
      g_list_model_get_n_items (self->model) == 0)
    return;

  // TODO: This should use the "content height"
  widget_height = gtk_widget_get_allocated_height (GTK_WIDGET (self));

  g_debug ("------------------------");
  g_debug ("        value: %f", gtk_adjustment_get_value (self->vadjustment));
  g_debug ("        upper: %f", gtk_adjustment_get_upper (self->vadjustment));
  g_debug ("    page_size: %f", gtk_adjustment_get_page_size (self->vadjustment));
  g_debug ("widget height: %d", widget_height);
  g_debug ("        bin_y: %d (SHOULD BE <= 0!)", bin_y (self));
  g_debug ("   bin_height: %d", bin_height (self));
  g_debug ("   bin_y_diff: %f", self->bin_y_diff);

  g_assert_cmpint (self->bin_y_diff, >=, 0);
  g_assert_cmpint (bin_height (self), >=, 0);
  double upper_before = estimated_list_height (self);

  double max_value = MAX (0, estimated_list_height (self) - widget_height);
  if (gtk_adjustment_get_value (self->vadjustment) > max_value)
    {
      /* We do NOT use _set_adjustment_value here since that would adjust the bin_y_diff
       * as well, which the later code will already to. */
      g_debug ("1 ###############################################################");
      g_signal_handler_block (self->vadjustment,
                              self->vadjustment_value_changed_id);
      gtk_adjustment_set_value (self->vadjustment, max_value);
      g_signal_handler_unblock (self->vadjustment,
                                self->vadjustment_value_changed_id);
      g_debug ("1 ###############################################################");
    }

  /* This "out of sight" case happens when the new value is so different from the old one
   * that we rather just remove all widgets and adjust the model_from/model_to values.
   * This happens when scrolling fast, clicking the scrollbar directly or just by programmatically
   * setting the vadjustment value.
   */
  if (bin_y (self) + bin_height (self) < 0 ||
      bin_y (self) >= widget_height)
    {
      int avg_row_height = estimated_row_height (self);
      double percentage;
      double value = gtk_adjustment_get_value (self->vadjustment);
      double upper = gtk_adjustment_get_upper (self->vadjustment);
      double page_size = gtk_adjustment_get_page_size (self->vadjustment);
      guint top_widget_index;
      int i;

      g_debug ("OUT OF SIGHT! bin_y: %d, bin_height; %d, widget_height: %d",
                 bin_y (self), bin_height (self), widget_height);
      g_debug ("Value: %f, upper: %f", value, upper);

      for (i = self->widgets->len - 1; i >= 0; i --)
        remove_child_by_index (self, i);

      g_assert (self->widgets->len == 0);

      percentage = value / (upper - page_size);

      top_widget_index = (guint) (g_list_model_get_n_items (self->model) * percentage);
      g_debug ("top_widget_index: %u (Percentage %f)", top_widget_index, percentage);

      if (top_widget_index > g_list_model_get_n_items (self->model))
        {
          g_assert (FALSE);
        }
      else
        {
          self->model_from = top_widget_index;
          self->model_to   = top_widget_index;
          self->bin_y_diff = self->model_from * avg_row_height;
        }

        g_assert (self->model_from <= g_list_model_get_n_items (self->model));
        g_assert (self->model_from <= self->model_to);
        g_assert (self->model_to <= g_list_model_get_n_items (self->model));
        g_assert (bin_y (self) <= widget_height);

        g_debug ("After OOS: model_from: %d, model_to: %d, bin_y: %d, bin_height; %d",
                   self->model_from, self->model_to, bin_y (self), bin_height (self));
    }


  /* It might be necessary to get back here... */
maybe_add_widgets:
  g_debug ("model_from: %u, model_to: %u", self->model_from, self->model_to);
  /* If we already show the last item, i.e. we are at the end of the list anyway,
   * BUT the last item is not allocated at the very bottom, we shift everything down here,
   * so the code later might add an item at the top.
   *
   * This can happen when one scrolled to the bottom and then increased the widget height.
   */
  if (self->model_to == g_list_model_get_n_items (self->model) &&
      self->model_from > 0 &&
      bin_y (self) + bin_height (self) < widget_height)
    {
      /*g_debug ("AT THE END");*/
      /*g_debug ("        bin_y: %d", bin_y (self));*/
      /*g_debug ("   bin_height: %d", bin_height (self));*/
      /*g_debug ("widget height: %d", widget_height);*/

      self->bin_y_diff += widget_height - (bin_y (self) + bin_height (self));

      /*g_debug ("bin_y_diff now: %f", self->bin_y_diff);*/
      /*g_debug ("         bin_y: %d", bin_y (self));*/
      /*g_debug ("    bin_height: %d", bin_height (self));*/
      /*g_debug (" widget height: %d", widget_height);*/

      g_assert (bin_y (self) + bin_height (self) >= widget_height);
    }
  else if (self->model_from == 0 && bin_y (self) > 0)
    {
      /* We are at the very top of the list (item 0 is shown), but we
       * allocate it at y > 0 because of a radical value/estimated-height change. */
      g_debug ("YEP!");
      self->bin_y_diff = 0;
      g_signal_handler_block (self->vadjustment,
                              self->vadjustment_value_changed_id);
      gtk_adjustment_set_value (self->vadjustment, 0);
      g_signal_handler_unblock (self->vadjustment,
                                self->vadjustment_value_changed_id);
      g_debug ("bin_y now: %d", bin_y (self));
    }

  /* Remove top widgets */
  {
    guint i;

    for (i = 0; i < self->widgets->len; i ++)
      {
        GtkWidget *w = g_ptr_array_index (self->widgets, i);
        int w_height = requested_row_height (self, w);
        if (bin_y (self) + row_y (self, i) + w_height < 0)
          {
            g_debug ("bin_y: %d, row_y: %d, w_height: %d", bin_y (self), row_y (self, i), w_height);
            g_assert_cmpint (i, ==, 0);
            self->bin_y_diff += w_height;
            remove_child_by_index (self, i);
            self->model_from ++;
            top_removed ++;
            g_debug ("Removing from top with index %u. bin_y_diff now: %f", i, self->bin_y_diff);

            /* Do the first row again */
            i--;
          } else
            break;
      }
  }

  /* Remove bottom widgets */
  {
    int i = self->widgets->len - 1;
    for (;;)
      {
        GtkWidget *w;
        int y;

        if (i <= 0)
          {
            break;
          }

        y = bin_y (self) + row_y (self, i);

        if (y < widget_height)
          {
            break;
          }
        g_debug ("Removing widget at bottom with y %d", y);

        w = g_ptr_array_index (self->widgets, i);
        g_assert (w);

        remove_child_by_index (self, i);
        self->model_to --;
        bottom_removed ++;

        i--;
      }
  }

  /* Add top widgets */
  {
    /*g_debug ("adding on top. bin_y: %d, bin_y_diff: %f",*/
               /*bin_y (self), self->bin_y_diff);*/
    for (;;)
      {
        GtkWidget *new_widget;
        int min;

        if (bin_y (self) <= 0)
          {
            break;
          }

        if (self->model_from == 0)
          {
            break;
          }

        self->model_from --;

        g_debug ("Adding on top for index %u", self->model_from);
        new_widget = get_widget (self, self->model_from);
        g_assert (new_widget != NULL);
        insert_child_internal (self, new_widget, 0);
        min = requested_row_height (self, new_widget);
        self->bin_y_diff -= min;
        top_added ++;
      }
    g_debug ("After adding on top. bin_y: %d, bin_y_diff: %f",
               bin_y (self), self->bin_y_diff);

    if (top_added > 0 && bin_y (self) > 0)
      goto maybe_add_widgets;

  }

  /* Insert bottom widgets */
  {
    for (;;)
      {
        GtkWidget *new_widget;

        /* If the widget is full anyway */
        if (bin_y (self) + bin_height (self) >= widget_height)
          {
            break;
          }

        /* ... or if we are out of items */
        if (self->model_to >= g_list_model_get_n_items (self->model))
          {
            break;
          }

        g_debug ("Adding at bottom for model index %u. bin_y: %d, bin_height: %d", self->model_to,
                   bin_y (self), bin_height (self));
        new_widget = get_widget (self, self->model_to);
        insert_child_internal (self, new_widget, self->widgets->len);

        self->model_to ++;
        bottom_added ++;
      }
  }

  g_debug ("Top removed:    %d", top_removed);
  g_debug ("Top added:      %d", top_added);
  g_debug ("Bottom removed: %d", bottom_removed);
  g_debug ("Bottom added:   %d", bottom_added);

  if (top_removed    > 0) g_assert_cmpint (top_added,      ==, 0);
  if (top_added      > 0) g_assert_cmpint (top_removed,    ==, 0);
  if (bottom_removed > 0) g_assert_cmpint (bottom_added,   ==, 0);
  if (bottom_added   > 0) g_assert_cmpint (bottom_removed, ==, 0);

  g_assert_cmpint (self->widgets->len, ==, self->model_to - self->model_from);

  /*
   * The configure_adjustment call will adjust the adjustment value if it's larger than the adjustment
   * upper. BUT the assumption here is the we scrolled to the end, to the new value will simply be
   * upper - page_size, which is wrong since it's value > upper - page_size because the upper changed
   * as a result of us adding/removing widgets.
   *
   * We need to handle this here, separately.
   */
  double value = gtk_adjustment_get_value (self->vadjustment);
  int new_upper = estimated_list_height (self);

  if (new_upper != (int)upper_before)
  /*if (value > new_upper - widget_height)*/
    {
      g_debug ("%d != %d", new_upper, (int)upper_before);
       /*g_debug ("%f > %f!", value, new_upper - widget_height);*/
      g_debug ("Value: %f, old upper: %f, new upper: %d, page_size: %d", value, upper_before, new_upper, widget_height);
      g_debug ("bin_y:      %d", bin_y (self));
      g_debug ("bin_y_diff: %f", self->bin_y_diff);

      int cur_bin_y = bin_y (self);
      int new_value = (self->model_from * estimated_row_height (self)) - cur_bin_y;
      new_value = MIN (new_value, new_upper - widget_height);
      new_value = MAX (new_value, 0);

      gtk_adjustment_set_upper (self->vadjustment, new_upper);
      set_vadjustment_value (self, new_value);
    }

  configure_adjustment (self);

  g_assert_cmpint (self->bin_y_diff, >=, 0);
  g_assert_cmpint (bin_y (self), <=, 0);

  if (self->model_from > 0 && self->model_to == g_list_model_get_n_items (self->model))
    g_assert (bin_y (self) + bin_height (self) >= widget_height);

}

static void
value_changed_cb (GtkAdjustment *adjustment,
                  gpointer       user_data)
{
  GdModelListBox *self = user_data;

  g_debug ("%s: %f -> %f", __FUNCTION__, self->last_value, gtk_adjustment_get_value (adjustment));

  self->last_value = gtk_adjustment_get_value (adjustment);
  g_debug ("QUEUE ALLOCATE");
  /* ensure_visible_widgets will be called from size_allocate */
  g_assert (GTK_IS_WIDGET (user_data));
  gtk_widget_queue_allocate (user_data);
}

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  gpointer    user_data)
{
  GdModelListBox *self = user_data;
  int i;

  g_debug ("%s: position %d, removed: %u, added: %u", __FUNCTION__, position, removed, added);

  /* If the change is out of our visible range anyway,
   * we don't care. */
  if (position > self->model_to)
    {
      configure_adjustment (self);
      return;
    }

  /* Empty the current view */
  for (i = self->widgets->len - 1; i >= 0; i --)
    remove_child_by_index (self, i);

  self->model_to = self->model_from;
  self->bin_y_diff = 0;

  /* Will end up calling ensure_widgets */
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

/* GtkWidget vfuncs {{{ */
static void
__size_allocate (GtkWidget           *widget,
                 const GtkAllocation *allocation,
                 int                  baseline,
                 GtkAllocation       *out_clip)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (widget);
  static int k =0;


  int this_k = k++;
  g_debug (__FUNCTION__);
  g_debug ("Start %s(%d)", __FUNCTION__, this_k);
  ensure_visible_widgets (self);

  if (self->widgets->len > 0)
    {
      GtkAllocation child_alloc;
      int y;

      /* Now actually allocate sizes to all the rows */
      y = bin_y (self);

      child_alloc.x = 0;
      child_alloc.width = allocation->width;

      Foreach_Row
        int h;

        gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, allocation->width,
                            &h, NULL, NULL, NULL);
        child_alloc.y = y;
        child_alloc.height = h;
        g_debug ("Allocation for row %u of %u: %d, %d, %d, %d",
                   i, self->widgets->len,
                   child_alloc.x,
                   child_alloc.y,
                   child_alloc.width,
                   child_alloc.height);
        gtk_widget_size_allocate (row, &child_alloc, -1, out_clip);

        y += h;
      }}
      /* configure_adjustment is being called from ensure_visible_widgets already */
    }

  g_debug ("End %s(%d)", __FUNCTION__, this_k);
}

static void
__snapshot (GtkWidget   *widget,
            GtkSnapshot *snapshot)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (widget);
  GtkAllocation alloc;

  /* TODO: This should be the content size */
  gtk_widget_get_allocation (widget, &alloc);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                            0, 0,
                            alloc.width,
                            alloc.height
                          ),
                          "GdModelListBox clip");

  Foreach_Row
    gtk_widget_snapshot_child (widget,
                               row,
                               snapshot);
  }}

  gtk_snapshot_pop (snapshot);
}

static void
__measure (GtkWidget      *widget,
           GtkOrientation  orientation,
           int             for_size,
           int            *minimum,
           int            *natural,
           int            *minimum_baseline,
           int            *natural_baseline)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int min_width = 0;
      int nat_width = 0;

      Foreach_Row
        int m, n;
        gtk_widget_measure (row, GTK_ORIENTATION_HORIZONTAL, -1,
                            &m, &n, NULL, NULL);
        min_width = MAX (min_width, m);
        nat_width = MAX (nat_width, n);
      }}

      *minimum = min_width;
      *natural = nat_width;
    }
  else /* VERTICAL */
    {
      *minimum = 1;
      *natural = 1;
    }
}
/* }}} */

/* GObject vfuncs {{{ */
static void
__set_property (GObject      *object,
                guint         prop_id,
                const GValue *value,
                GParamSpec   *pspec)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (object);

  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
        g_set_object (&self->hadjustment, g_value_get_object (value));
        break;
      case PROP_VADJUSTMENT:
        if (self->vadjustment)
          {
            g_signal_handler_disconnect (self->vadjustment,
                                         self->vadjustment_value_changed_id);
          }
        g_set_object (&self->vadjustment, g_value_get_object (value));
        if (g_value_get_object (value))
          {
            self->last_value = gtk_adjustment_get_value (self->vadjustment);
            self->vadjustment_value_changed_id =
              g_signal_connect (G_OBJECT (self->vadjustment), "value-changed",
                                G_CALLBACK (value_changed_cb), object);
          }
        break;
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        break;
    }
}

static void
__get_property (GObject    *object,
                guint       prop_id,
                GValue     *value,
                GParamSpec *pspec)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (object);

  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
        g_value_set_object (value, self->hadjustment);
        break;
      case PROP_VADJUSTMENT:
        g_value_set_object (value, self->vadjustment);
        break;
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        break;
    }
}

static void
__finalize (GObject *obj)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (obj);
  guint i;

  g_debug ("LISTBOX FINALIZE. Pool: %u, widgets: %u", self->pool->len, self->widgets->len);

  for (i = 0; i < self->pool->len; i ++)
    {
      gtk_widget_unparent (g_ptr_array_index (self->pool, i));
      g_object_unref (g_ptr_array_index (self->pool, i));
    }

  for (i = 0; i < self->widgets->len; i ++)
    {
      gtk_widget_unparent (g_ptr_array_index (self->widgets, i));
      g_object_unref (g_ptr_array_index (self->widgets, i));
    }

  g_ptr_array_free (self->pool, TRUE);
  g_ptr_array_free (self->widgets, TRUE);

  g_clear_object (&self->hadjustment);
  g_clear_object (&self->vadjustment);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (gd_model_list_box_parent_class)->finalize (obj);
}
/* }}} */

GtkWidget *
gd_model_list_box_new (void)
{
  return GTK_WIDGET (g_object_new (GD_TYPE_MODEL_LIST_BOX, NULL));
}

void
gd_model_list_box_set_fill_func (GdModelListBox         *self,
                                 GdModelListBoxFillFunc  func,
                                 gpointer                user_data)
{
  self->fill_func = func;
  self->fill_func_data = user_data;
}

void
gd_model_list_box_set_remove_func (GdModelListBox           *self,
                                   GdModelListBoxRemoveFunc  func,
                                   gpointer                  user_data)

{
  self->remove_func = func;
  self->remove_func_data = user_data;
}

void
gd_model_list_box_set_model (GdModelListBox *self,
                             GListModel     *model)
{
  g_return_if_fail (GD_IS_MODEL_LIST_BOX (self));

  if (self->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->model,
                                            G_CALLBACK (items_changed_cb), self);
      g_object_unref (self->model);
    }

  self->model = model;
  if (model != NULL)
    {
      g_signal_connect (G_OBJECT (model), "items-changed", G_CALLBACK (items_changed_cb), self);
      g_object_ref (model);
    }

  ensure_visible_widgets (self);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

GListModel *
gd_model_list_box_get_model (GdModelListBox *self)
{
  return self->model;
}

static void
gd_model_list_box_class_init (GdModelListBoxClass *class)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass    *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = __set_property;
  object_class->get_property = __get_property;
  object_class->finalize     = __finalize;

  widget_class->measure       = __measure;
  widget_class->size_allocate = __size_allocate;
  widget_class->snapshot      = __snapshot;

  g_object_class_override_property (object_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  gtk_widget_class_set_css_name (widget_class, "list");
}

static void
gd_model_list_box_init (GdModelListBox *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->widgets    = g_ptr_array_sized_new (20);
  self->pool       = g_ptr_array_sized_new (10);
  self->model_from = 0;
  self->model_to   = 0;
  self->bin_y_diff = 0;
}
