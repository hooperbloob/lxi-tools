/*
 * Copyright (c) 2022  Martin Lund
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gtkchart.h"
#include <gsk/gl/gskglrenderer.h>

struct chart_point_t
{
  double x;
  double y;
};

struct _GtkChart
{
  GtkWidget parent_instance;
  int type;
  char *title;
  char *label;
  char *x_label;
  char *y_label;
  double x_max;
  double y_max;
  double value;
  double value_min;
  double value_max;
  int width;
  void *user_data;
  GSList *point_list;
  GtkSnapshot *snapshot;
};

struct _GtkChartClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkChart, gtk_chart, GTK_TYPE_WIDGET)

static void gtk_chart_init (GtkChart *self)
{
  // Defaults
  self->type = GTK_CHART_TYPE_LINE;
  self->title = NULL;
  self->label = NULL;
  self->x_label = NULL;
  self->y_label = NULL;
  self->x_max = 100;
  self->y_max = 100;
  self->value_min = 0;
  self->value_max = 100;
  self->width = 500;
  self->snapshot = NULL;

  //gtk_widget_init_template (GTK_WIDGET (self));
}

static void gtk_chart_dispose (GObject *object)
{
  GtkChart *self = GTK_CHART_WIDGET (object);

  // Cleanup
  g_free(self->title);
  g_free(self->label);
  g_free(self->x_label);
  g_free(self->y_label);

  gdk_display_sync(gdk_display_get_default());

  g_slist_free_full(g_steal_pointer(&self->point_list), g_free);
  g_slist_free(self->point_list);

  G_OBJECT_CLASS (gtk_chart_parent_class)->dispose (object);
}

void chart_draw_line_or_scatter(GtkChart *self,
                                GtkSnapshot *snapshot,
                                float h,
                                float w)
{
  GdkRGBA bg_color, white, blue, red, line, grid;
  cairo_text_extents_t extents;
  char value[20];

  //gdk_rgba_parse (&bg_color, "#2d2d2d");
  gdk_rgba_parse (&bg_color, "black");
  gdk_rgba_parse (&white, "rgba(255,255,255,0.75)");
  gdk_rgba_parse (&blue, "blue");
  gdk_rgba_parse (&red, "red");
  gdk_rgba_parse (&line, "#325aad");
  gdk_rgba_parse (&grid, "rgba(255,255,255,0.1)");

  // Set background color
  gtk_snapshot_append_color (snapshot,
                             &bg_color,
                             &GRAPHENE_RECT_INIT(0, 0, w, h));

  // Assume aspect ratio w:h = 2:1

  // Set up Cairo region
  cairo_t * cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT(0, 0, w, h));
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_FAST);
  cairo_set_tolerance (cr, 1.5);
  gdk_cairo_set_source_rgba (cr, &white);
  cairo_select_font_face (cr, "Ubuntu", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

  // Move coordinate system to bottom left
  cairo_translate(cr, 0, h);

  // Invert y-axis
  cairo_scale(cr, 1, -1);

  // Draw title
  cairo_set_font_size (cr, 15.0 * (w/650));
  cairo_text_extents(cr, self->title, &extents);
  cairo_move_to (cr, 0.5 * w - extents.width/2, 0.9 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, self->title);
  cairo_restore(cr);

  // Draw x-axis label
  cairo_set_font_size (cr, 11.0 * (w/650));
  cairo_text_extents(cr, self->x_label, &extents);
  cairo_move_to (cr, 0.5 * w - extents.width/2, 0.075 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, self->x_label);
  cairo_restore(cr);

  // Draw y-axis label
  cairo_text_extents(cr, self->y_label, &extents);
  cairo_move_to (cr, 0.035 * w, 0.5 * h - extents.width/2);
  cairo_save(cr);
  cairo_rotate(cr, M_PI/2);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, self->y_label);
  cairo_restore(cr);

  // Draw x-axis
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.1 * w, 0.2 * h);
  cairo_line_to (cr, 0.9 * w, 0.2 * h);
  cairo_stroke (cr);

  // Draw y-axis
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.1 * w, 0.8 * h);
  cairo_line_to (cr, 0.1 * w, 0.2 * h);
  cairo_stroke (cr);

  // Draw x-axis value at 100% mark
  g_snprintf(value, sizeof(value), "%.1f", self->x_max);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.9 * w - extents.width/2, 0.16 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw x-axis value at 75% mark
  g_snprintf(value, sizeof(value), "%.1f", (self->x_max/4) * 3);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.7 * w - extents.width/2, 0.16 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw x-axis value at 50% mark
  g_snprintf(value, sizeof(value), "%.1f", self->x_max/2);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.5 * w - extents.width/2, 0.16 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw x-axis value at 25% mark
  g_snprintf(value, sizeof(value), "%.1f", self->x_max/4);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.3 * w - extents.width/2, 0.16 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw x-axis value at 0% mark
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, "0", &extents);
  cairo_move_to (cr, 0.1 * w - extents.width/2, 0.16 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, "0");
  cairo_restore(cr);

  // Draw y-axis value at 0% mark
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, "0", &extents);
  cairo_move_to (cr, 0.091 * w - extents.width, 0.191 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, "0");
  cairo_restore(cr);

  // Draw y-axis value at 25% mark
  g_snprintf(value, sizeof(value), "%.1f", self->y_max/4);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.091 * w - extents.width, 0.34 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw y-axis value at 50% mark
  g_snprintf(value, sizeof(value), "%.1f", self->y_max/2);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.091 * w - extents.width, 0.49 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw y-axis value at 75% mark
  g_snprintf(value, sizeof(value), "%.1f", (self->y_max/4) * 3);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.091 * w - extents.width, 0.64 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Draw y-axis value at 100% mark
  g_snprintf(value, sizeof(value), "%.1f", self->y_max);
  cairo_set_font_size (cr, 8.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to (cr, 0.091 * w - extents.width, 0.79 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, value);
  cairo_restore(cr);

  // Set color of grid
  gdk_cairo_set_source_rgba (cr, &grid);

  // Draw grid x-line 25%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.1 * w, 0.35 * h);
  cairo_line_to (cr, 0.9 * w, 0.35 * h);
  cairo_stroke (cr);

  // Draw grid x-line 50%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.1 * w, 0.5 * h);
  cairo_line_to (cr, 0.9 * w, 0.5 * h);
  cairo_stroke (cr);

  // Draw grid x-line 75%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.1 * w, 0.65 * h);
  cairo_line_to (cr, 0.9 * w, 0.65 * h);
  cairo_stroke (cr);

  // Draw grid x-line 100%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.1 * w, 0.8 * h);
  cairo_line_to (cr, 0.9 * w, 0.8 * h);
  cairo_stroke (cr);

  // Draw grid y-line 25%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.3 * w, 0.8 * h);
  cairo_line_to (cr, 0.3 * w, 0.2 * h);
  cairo_stroke (cr);

  // Draw grid y-line 50%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.5 * w, 0.8 * h);
  cairo_line_to (cr, 0.5 * w, 0.2 * h);
  cairo_stroke (cr);

  // Draw grid y-line 75%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.7 * w, 0.8 * h);
  cairo_line_to (cr, 0.7 * w, 0.2 * h);
  cairo_stroke (cr);

  // Draw grid y-line 100%
  cairo_set_line_width (cr, 1);
  cairo_move_to (cr, 0.9 * w, 0.8 * h);
  cairo_line_to (cr, 0.9 * w, 0.2 * h);
  cairo_stroke (cr);

  // Move coordinate system to (0,0) of drawn gtk_chart
  cairo_translate(cr, 0.1 * w, 0.2 * h);
  gdk_cairo_set_source_rgba (cr, &line);
  cairo_set_line_width (cr, 2.0);

  // Calc scales
  float x_scale = (w - 2 * 0.1 * w) / self->x_max;
  float y_scale = (h - 2 * 0.2 * h) / self->y_max;

  // Draw data points from list
  GSList *l;
  for (l = self->point_list; l != NULL; l = l->next)
  {
    struct chart_point_t *point = l->data;

    switch (self->type)
    {
      case GTK_CHART_TYPE_LINE:
        if (l == self->point_list)
        {
          // Move to first point
          cairo_move_to(cr, point->x * x_scale, point->y * y_scale);
        }
        else
        {
          // Draw line to next point
          cairo_line_to(cr, point->x * x_scale, point->y * y_scale);
          cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
          cairo_stroke(cr);
          cairo_move_to(cr, point->x * x_scale, point->y * y_scale);
        }
        break;

      case GTK_CHART_TYPE_SCATTER:
        // Draw square
        //cairo_rectangle (cr, point->x * x_scale, point->y * y_scale, 4, 4);
        //cairo_fill(cr);

        // Draw point
        cairo_set_line_width(cr, 4);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, point->x * x_scale, point->y * y_scale);
        cairo_close_path (cr);
        cairo_stroke (cr);
        break;
    }
  }

  cairo_destroy (cr);
}

void chart_draw_number(GtkChart *self,
                       GtkSnapshot *snapshot,
                       float h,
                       float w)
{
  GdkRGBA bg_color, white, blue, red, line, grid;
  cairo_text_extents_t extents;
  char value[20];

  gdk_rgba_parse (&bg_color, "black");
  gdk_rgba_parse (&white, "rgba(255,255,255,0.75)");
  gdk_rgba_parse (&blue, "blue");
  gdk_rgba_parse (&red, "red");
  gdk_rgba_parse (&line, "#325aad");
  gdk_rgba_parse (&grid, "rgba(255,255,255,0.1)");

  // Set background color
  gtk_snapshot_append_color (snapshot,
                             &bg_color,
                             &GRAPHENE_RECT_INIT(0, 0, w, h));

  // Assume aspect ratio w:h = 1:1

  // Set up Cairo region
  cairo_t * cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT(0, 0, w, h));
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_FAST);
  cairo_set_tolerance (cr, 1.5);
  gdk_cairo_set_source_rgba (cr, &white);
  cairo_select_font_face (cr, "Ubuntu", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

  // Move coordinate system to bottom left
  cairo_translate(cr, 0, h);

  // Invert y-axis
  cairo_scale(cr, 1, -1);

  // Draw title
  cairo_set_font_size (cr, 15.0 * (w/650));
  cairo_text_extents(cr, self->title, &extents);
  cairo_move_to (cr, 0.5 * w - extents.width/2, 0.9 * h);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text (cr, self->title);
  cairo_restore(cr);

  // Draw label
  cairo_set_font_size (cr, 25.0 * (w/650));
  cairo_text_extents(cr, self->label, &extents);
  cairo_move_to(cr, 0.5 * w - extents.width/2, 0.2 * h - extents.height/2);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text(cr, self->label);
  cairo_restore(cr);

  // Draw number
  g_snprintf(value, sizeof(value), "%.1f", self->value);
  cairo_set_font_size (cr, 140.0 * (w/650));
  cairo_text_extents(cr, value, &extents);
  cairo_move_to(cr, 0.5 * w - extents.width/2, 0.5 * h - extents.height/2);
  cairo_save(cr);
  cairo_scale(cr, 1, -1);
  cairo_show_text(cr, value);
  cairo_restore(cr);

  cairo_destroy (cr);
}

static void gtk_chart_snapshot (GtkWidget   *widget,
                                GtkSnapshot *snapshot)
{
  GtkChart *self = GTK_CHART_WIDGET(widget);

  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);

  // Draw various chart types
  switch (self->type)
  {
    case GTK_CHART_TYPE_LINE:
    case GTK_CHART_TYPE_SCATTER:
      chart_draw_line_or_scatter(self, snapshot, height, width);
      break;
    case GTK_CHART_TYPE_NUMBER:
      chart_draw_number(self, snapshot, height, width);
      break;
  }

  self->snapshot = snapshot;
}

static void gtk_chart_class_init (GtkChartClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_chart_dispose;
  
  widget_class->snapshot = gtk_chart_snapshot;
}

GtkWidget * gtk_chart_new (void)
{
  GtkChart *self;

  self = g_object_new (GTK_CHART_TYPE_WIDGET, NULL);

  return GTK_WIDGET (self);  
}

void gtk_chart_set_user_data(GtkChart *chart, void *user_data)
{
  chart->user_data = user_data;
}

void * gtk_chart_get_user_data(GtkChart *chart)
{
  return chart->user_data;
}

void gtk_chart_set_type(GtkChart *chart, int type)
{
  chart->type = type;
}

void gtk_chart_set_title(GtkChart *chart, const char *title)
{
  chart->title = g_strdup(title);
}

void gtk_chart_set_label(GtkChart *chart, const char *label)
{
  chart->label = g_strdup(label);
}

void gtk_chart_set_x_label(GtkChart *chart, const char *x_label)
{
  chart->x_label = g_strdup(x_label);
}

void gtk_chart_set_y_label(GtkChart *chart, const char *y_label)
{
  chart->y_label = g_strdup(y_label);
}

void gtk_chart_set_x_max(GtkChart *chart, double x_max)
{
  chart->x_max = x_max;
}

void gtk_chart_set_y_max(GtkChart *chart, double y_max)
{
  chart->y_max = y_max;
}

void gtk_chart_set_width(GtkChart *chart, int width)
{
  chart->width = width;
}

void gtk_chart_plot_point(GtkChart *chart, double x, double y)
{
  // Allocate memory for new point
  struct chart_point_t *point = g_new0(struct chart_point_t, 1);
  point->x = x;
  point->y = y;

  // Add point to list to be drawn
  chart->point_list = g_slist_append(chart->point_list, point);

  // Queue draw of widget
  gtk_widget_queue_draw(GTK_WIDGET(chart));
}

void gtk_chart_set_value(GtkChart *chart, double value)
{
  chart->value = value;

  // Queue draw of widget
  gtk_widget_queue_draw(GTK_WIDGET(chart));
}

void gtk_chart_set_value_min(GtkChart *chart, double value)
{
  chart->value_min = value;
}

void gtk_chart_set_value_max(GtkChart *chart, double value)
{
  chart->value_max = value;
}

bool gtk_chart_save_csv(GtkChart *chart, const char *filename)
{
  struct chart_point_t *point;
  GSList *l;

  // Open file
  FILE *file = fopen(filename, "w"); // write only

  if (file == NULL)
  {
    g_print("Error: Could not open file\n");
    return false;
  }

  // Write CSV data
  for (l = chart->point_list; l != NULL; l = l->next)
  {
    point = l->data;
    fprintf(file, "%f, %f\n", point->x, point->y);
  }

  // Close file
  fclose(file);

  return true;
}

bool gtk_chart_save_image(GtkChart *chart, const char *filename)
{
  int width = gtk_widget_get_width (GTK_WIDGET(chart));
  int height = gtk_widget_get_height (GTK_WIDGET(chart));

  // Get to the PNG image file from paintable
  GdkPaintable *paintable = gtk_widget_paintable_new (GTK_WIDGET(chart));
  GtkSnapshot *snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  GskRenderNode *node = gtk_snapshot_to_node (snapshot);
  GskRenderer *renderer = gsk_gl_renderer_new();
  gsk_renderer_realize (renderer, NULL, NULL);
  GdkTexture *texture = gsk_renderer_render_texture (renderer, node, NULL);
  gdk_texture_save_to_png (texture, filename);

  // Cleanup
  g_object_unref(paintable);
  g_object_unref(snapshot);
  gsk_render_node_unref(node);
  g_object_unref(renderer);
  g_object_unref(texture);

  return true;
}
