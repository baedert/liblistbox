#include <glib.h>
#include "gd-model-list-box.h"

#define ROW_WIDTH  300
#define ROW_HEIGHT 100

static GtkWidget *
label_from_label (gpointer  item,
                  GtkWidget *widget,
                  guint      item_index,
                  gpointer   user_data)
{
  GtkWidget *widget_item = item;
  GtkWidget *new_widget;

  g_assert (GTK_IS_LABEL (widget_item));

  if (widget)
    new_widget = widget;
  else
    new_widget = gtk_label_new ("");

  g_assert (GTK_IS_LABEL (new_widget));
  gpointer data = g_object_get_data (G_OBJECT (widget_item), "height");

  if (data != NULL)
    gtk_widget_set_size_request (new_widget, ROW_WIDTH, GPOINTER_TO_INT (data));
  else
    gtk_widget_set_size_request (new_widget, ROW_WIDTH, ROW_HEIGHT);

  gtk_label_set_label (GTK_LABEL (new_widget), gtk_label_get_label (GTK_LABEL (widget_item)));

  return new_widget;
}


static void
simple (void)
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  GtkWidget *w;
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  w = gtk_label_new ("Some Text");
  g_list_store_append (store, w);
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;

  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);
  /* We always request the minimum height */
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1);
  g_assert_cmpint (nat, ==, 1);

  /* Width should be one row now though */
  gtk_widget_measure (listbox, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, ROW_WIDTH);
  g_assert_cmpint (nat, ==, ROW_WIDTH);

  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  /* 20 all in all */
  for (i = 0; i < 19; i++)
    {
      w = gtk_label_new ("Some Text");
      g_list_store_append (store, w);
    }
  gtk_widget_measure (listbox, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);
  g_assert_cmpint (min, ==, ROW_WIDTH);
  g_assert_cmpint (nat, ==, ROW_WIDTH);

  g_assert_cmpint ((int)gtk_adjustment_get_upper (vadjustment), ==, 20 * ROW_HEIGHT);

  // Didn't scroll yet...
  g_assert_cmpint ((int)gtk_adjustment_get_value (vadjustment), ==, 0);

  double new_value = gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment);
  gtk_adjustment_set_value (vadjustment, new_value);
  g_assert_cmpint ((int)gtk_adjustment_get_value (vadjustment), ==, (int)new_value);

  // Note that we just changed the adjustment's value, but changing the value only
  // calls gtk_widget_queue_allocate, i.e. the changes do not immediately take effect.
  // We have to wait for a size-allocate to actually observe the changes.
  // In an actual application, this would happen in the same frame during the layout phase.


  // The allocated height of the list should be the exact same as the
  // scrolledwindow (provided css doesn't fuck it up), which is 500px.
  // Every row is 100px so there should now be 5 of those.
  g_assert_cmpint (GD_MODEL_LIST_BOX (listbox)->widgets->len, ==, 5);

  // Force size-allocating all rows after the scrolling above.
  // This works out because changing the adjustment's value will cause a queue_allocate.
  gtk_widget_size_allocate (listbox, &fake_alloc, -1, &fake_clip);

  // So, the last one should be allocated at the very bottom of the listbox, nowhere else.
  GtkWidget *last_row = g_ptr_array_index (GD_MODEL_LIST_BOX (listbox)->widgets,
                                           GD_MODEL_LIST_BOX (listbox)->widgets->len - 1);
  g_assert_nonnull (last_row);
  g_assert (gtk_widget_get_parent (last_row) == listbox);
  GtkAllocation row_alloc;
  gtk_widget_get_allocation (last_row, &row_alloc);

  // Widget allocations in gtk4 are parent relative so this works out.
  g_assert_cmpint (row_alloc.y, ==, gtk_widget_get_allocated_height (listbox) - ROW_HEIGHT);


  g_object_unref (scroller);
}


/*
 * Here we try to reproduce a nasty edge case.
 * The rows at the top of the list are pretty high, so the listbox overestimates
 * the height. When clicking on the lower part of the scrollbar, we immediately jump to
 * the bottom of the listbox, using a very large value for the vadjustment.
 *
 * However, once we recreate the widgets at the bottom of the listbox, we realize that
 * the value does not even make sense anymore and we need to readjust it to fit the now
 * different estimated widget height.
 *
 * This means a change in the adjustment's value causes a change in both the its value
 * and its upper. Still, it should not end in an infinite loop of course. At the same time,
 * the rows should all be properly allocated, especially the last row should be at the
 * very bottom of the listbox.
 */
static void
overscroll (void)
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  // First 10 large rows
  for (i = 0; i < 10; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT * 10));
      g_list_store_append (store, w);
    }

  // Now 10 smaller ones
  for (i = 0; i < 10; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT));
      g_list_store_append (store, w);
    }

  // Note that the 10 large rows are plenty enough to cover the entire widget allocation
  // we are about to use, so only they will affect the estimated ilst height, not the
  // smaller ones.
  //
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // We should have configured the adjustment by now, which will
  // result in 20 * (ROW_HEIGHT * 10) as the estimated upper
  g_assert_cmpint ((int)gtk_adjustment_get_upper (vadjustment), ==, 20 * ROW_HEIGHT * 10);

  // Lets scroll to the very bottom
  /*g_message ("-------------------------------------------------");*/
  double new_value = gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment);
  gtk_adjustment_set_value (vadjustment, new_value);

  // This assertion temporarily holds until the next size_allocate.
  g_assert_cmpint ((int)gtk_adjustment_get_value (vadjustment), ==, (int)new_value);

  // Same size as before, we just want to execute the listbox's size-allocate, which
  // works since changing the adjustment value just means a queue_allocate
  gtk_widget_size_allocate (listbox, &fake_alloc, -1, &fake_clip);
  /*g_message ("-------------------------------------------------");*/

  // This will reconfigure the adjustment, now with all small rows
  g_assert_cmpint ((int)gtk_adjustment_get_upper (vadjustment), ==, 20 * ROW_HEIGHT);

  // But what's the adjustment value? It can't be the one we just set anymore, that's way too much
  // given the new estimated list height. Since we scrolled to the very bottom,
  // it should obviously equal to the usual upper - page_size.
  double cur_value = gtk_adjustment_get_value (vadjustment);
  double upper = gtk_adjustment_get_upper (vadjustment);
  double page_size = gtk_adjustment_get_page_size (vadjustment);
  g_assert_cmpint ((int)cur_value, ==, (int)upper - (int)page_size);

  GtkWidget *last_row = g_ptr_array_index (GD_MODEL_LIST_BOX (listbox)->widgets,
                                           GD_MODEL_LIST_BOX (listbox)->widgets->len - 1);
  g_assert_nonnull (last_row);
  g_assert (gtk_widget_get_parent (last_row) == listbox);
  GtkAllocation row_alloc;
  gtk_widget_get_allocation (last_row, &row_alloc);
  g_assert_cmpint (row_alloc.y, ==, gtk_widget_get_allocated_height (listbox) - ROW_HEIGHT);


  /* Twice, just making sure we're done... */
  gtk_widget_size_allocate (listbox, &fake_alloc, -1, &fake_clip);
  gtk_widget_size_allocate (listbox, &fake_alloc, -1, &fake_clip);


  cur_value = gtk_adjustment_get_value (vadjustment);
  upper = gtk_adjustment_get_upper (vadjustment);
  page_size = gtk_adjustment_get_page_size (vadjustment);
  g_assert_cmpint ((int)cur_value, ==, (int)upper - (int)page_size);

  last_row = g_ptr_array_index (GD_MODEL_LIST_BOX (listbox)->widgets,
                                GD_MODEL_LIST_BOX (listbox)->widgets->len - 1);
  g_assert_nonnull (last_row);
  g_assert (gtk_widget_get_parent (last_row) == listbox);
  gtk_widget_get_allocation (last_row, &row_alloc);
  g_assert_cmpint (row_alloc.y, ==, gtk_widget_get_allocated_height (listbox) - ROW_HEIGHT);
}

static void
scrolling (void)
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  // First 10 large rows
  for (i = 0; i < 10; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT * 10));
      g_list_store_append (store, w);
    }

  // Now 10 smaller ones
  for (i = 0; i < 10; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT));
      g_list_store_append (store, w);
    }

  // Note that the 10 large rows are plenty enough to cover the entire widget allocation
  // we are about to use, so only they will affect the estimated ilst height, not the
  // smaller ones.
  //
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // We should have configured the adjustment by now, which will
  // result in 20 * (ROW_HEIGHT * 10) as the estimated upper
  g_assert_cmpint ((int)gtk_adjustment_get_upper (vadjustment), ==, 20 * ROW_HEIGHT * 10);


  /* Just scroll to the bottom and check that none of the assertions
   * in GdModelListBox itself trigger. */
  while (gtk_adjustment_get_value (vadjustment) <
         gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment))
    {
      double v = gtk_adjustment_get_value (vadjustment);

      gtk_adjustment_set_value (vadjustment, v + 1.0);

      gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
      gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);
    }
  // We are now at the bottom, so...
  g_assert_cmpint (GD_MODEL_LIST_BOX (listbox)->model_to, ==, g_list_model_get_n_items (G_LIST_MODEL (store)));
  GtkWidget *last_row = g_ptr_array_index (GD_MODEL_LIST_BOX (listbox)->widgets,
                                           GD_MODEL_LIST_BOX (listbox)->widgets->len - 1);
  g_assert_nonnull (last_row);
  g_assert (gtk_widget_get_parent (last_row) == listbox);
  GtkAllocation row_alloc;
  gtk_widget_get_allocation (last_row, &row_alloc);
  g_assert_cmpint (row_alloc.y, ==, gtk_widget_get_allocated_height (listbox) - ROW_HEIGHT);


  // And up again...
  while (gtk_adjustment_get_value (vadjustment) > 0)
    {
      double v = gtk_adjustment_get_value (vadjustment);

      gtk_adjustment_set_value (vadjustment, v - 1.0);

      gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
      gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);
    }
  // And now at the top, so...
  g_assert_cmpint (GD_MODEL_LIST_BOX (listbox)->model_from, ==, 0);
  last_row = g_ptr_array_index (GD_MODEL_LIST_BOX (listbox)->widgets, 0);
  g_assert_nonnull (last_row);
  g_assert (gtk_widget_get_parent (last_row) == listbox);
  gtk_widget_get_allocation (last_row, &row_alloc);
  g_assert_cmpint (row_alloc.y, ==, 0);
}

static void
overscroll_top (void)
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  // First 1 small row
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT));
      g_list_store_append (store, w);
    }

  // Now 9 larger rows
  for (i = 0; i < 9; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT * 5));
      g_list_store_append (store, w);
    }
  g_assert (g_list_model_get_n_items (G_LIST_MODEL (store)) == 10);

  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // We have one small row on top and several larger ones below that.

  // First, scroll the first row out of view so the current estimated list height is
  // exclusively controled by the larger rows
  gtk_adjustment_set_value (vadjustment, ROW_HEIGHT + 5);
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // Now, the estimated list size should be n * (5 * ROW_HEIGHT).
  g_assert_cmpint ((int)gtk_adjustment_get_upper (vadjustment), ==, 10 * (5 * ROW_HEIGHT));

  // Since we know the estimated size of the list is the above, the new value should be
  // "one row missing plus 5 px".
  // The listbox now assumes that every row is 500px high, as every existing row is.
  g_assert_cmpint ((int)gtk_adjustment_get_value (vadjustment), ==, 505);

  // SO! If we now scroll up and bring the old row into view again, what's gonna happen?
  // Specifically, we scroll up by more than the first rows height + 5, causing the listbox
  // to overscroll at the top.
  //
  // This SHOULD lead to a normal state... If the listbox is unable to repair its
  // mis-estimation, the very first row will be allocated at a y > 0, which is wrong.
  gtk_adjustment_set_value (vadjustment,
                            gtk_adjustment_get_value (vadjustment) - (ROW_HEIGHT + 10));
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  g_object_unref (G_OBJECT (scroller));
}

static void
scroll_to_bottom_resize (void)
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  for (i = 0; i < 10; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT));
      g_list_store_append (store, w);
    }
  g_assert (g_list_model_get_n_items (G_LIST_MODEL (store)) == 10);

  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // Scroll to the bottom
  gtk_adjustment_set_value (vadjustment,
                            gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment));

  // This should not change anything...
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // Now, add one pixel height, causing the items to be allocated one pixel too far at the top,
  // i.e. the last row is not allocated at the very bottom of the list...
  // Unless the listbox fixes this
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  fake_alloc.height += 1;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // Now fuck it really up
  while (fake_alloc.height < 2000)
    {
      gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
      fake_alloc.height += 1;
      gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);
    }

  g_object_unref (G_OBJECT (scroller));
}

static void
model_change (void)
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  for (i = 0; i < 10; i ++)
    {
      GtkWidget *w = gtk_label_new ("FOO!");
      g_object_set_data (G_OBJECT (w), "height", GINT_TO_POINTER (ROW_HEIGHT));
      g_list_store_append (store, w);
    }
  g_assert (g_list_model_get_n_items (G_LIST_MODEL (store)) == 10);

  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // We are at the very top, the last item is NOT visible, so remove it and see what happens...
  g_list_store_remove (store, g_list_model_get_n_items (G_LIST_MODEL (store)) - 1);
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);

  // But the first item is clearly visible, what happens if we remove that one?
  g_list_store_remove (store, 0);
  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);
}

int
main (int argc, char **argv)
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/listbox/simple", simple);
  g_test_add_func ("/listbox/overscroll", overscroll);
  g_test_add_func ("/listbox/scrolling", scrolling);
  g_test_add_func ("/listbox/scroll-to-bottom-resize", scroll_to_bottom_resize);
  g_test_add_func ("/listbox/overscroll_top", overscroll_top);
  g_test_add_func ("/listbox/model-change", model_change);

  return g_test_run ();
}
