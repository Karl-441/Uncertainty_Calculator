#include <gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// 数据存储结构
typedef struct {
    double *data;
    int count;
} MeasurementData;

// B类不确定度参数
typedef struct {
    double limit_error;       // 极限误差
    int distribution_type;    // 分布类型：0-均匀分布，1-正态分布(95%)，2-正态分布(99%)
} BTypeParams;

// 全局变量
MeasurementData meas_data = {NULL, 0};
BTypeParams b_params = {0.0, 0};  // 默认均匀分布
GtkWidget *data_entry;
GtkWidget *result_textview;
GtkListStore *data_liststore;     // 数据列表存储
GtkWidget *b_error_entry;         // B类极限误差输入
GtkWidget *dist_combo;            // 分布类型选择

// 计算平均值
double calculate_mean(MeasurementData *data) {
    if (data->count == 0) return 0.0;
    
    double sum = 0.0;
    for (int i = 0; i < data->count; i++) {
        sum += data->data[i];
    }
    return sum / data->count;
}

// 计算实验标准差（A类基础）
double calculate_std_dev(MeasurementData *data, double mean) {
    if (data->count <= 1) return 0.0;
    
    double sum_sq = 0.0;
    for (int i = 0; i < data->count; i++) {
        double diff = data->data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / (data->count - 1));
}

// 计算A类标准不确定度（平均值的标准不确定度）
double calculate_type_a_uncertainty(MeasurementData *data, double std_dev) {
    if (data->count == 0) return 0.0;
    return std_dev / sqrt(data->count);
}

// 计算B类标准不确定度
double calculate_type_b_uncertainty(BTypeParams *params) {
    if (params->limit_error <= 0) return 0.0;
    
    // 不同分布的包含因子k（符合GB/T 27411-2012）
    double k;
    switch (params->distribution_type) {
        case 0:  // 均匀分布
            k = sqrt(3);  // ≈1.732
            break;
        case 1:  // 正态分布（95%置信）
            k = 2.0;
            break;
        case 2:  // 正态分布（99%置信）
            k = 3.0;
            break;
        default:
            k = sqrt(3);
    }
    return params->limit_error / k;
}

// 计算合成标准不确定度
double calculate_combined_uncertainty(double u_a, double u_b) {
    return sqrt(u_a * u_a + u_b * u_b);
}

// 添加数据点并更新列表
void add_data_point(GtkWidget *widget, gpointer user_data) {
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(data_entry));
    if (text && *text) {
        double value = atof(text);
        
        // 重新分配内存存储数据
        double *new_data = realloc(meas_data.data, (meas_data.count + 1) * sizeof(double));
        if (new_data) {
            meas_data.data = new_data;
            meas_data.data[meas_data.count] = value;
            
            // 更新列表显示
            GtkTreeIter iter;
            gtk_list_store_append(data_liststore, &iter);
            gtk_list_store_set(data_liststore, &iter,
                              0, meas_data.count + 1,  // 序号
                              1, value,                // 数据值
                              -1);
            
            meas_data.count++;
            gtk_entry_set_text(GTK_ENTRY(data_entry), "");
        }
    }
}

// 计算不确定度（包含A类和B类）
void calculate_uncertainty(GtkWidget *widget, gpointer user_data) {
    if (meas_data.count < 2) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(user_data),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_OK,
                                                  "请至少输入2个数据点");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // 读取B类参数
    const gchar *b_error_text = gtk_entry_get_text(GTK_ENTRY(b_error_entry));
    b_params.limit_error = atof(b_error_text);
    b_params.distribution_type = gtk_combo_box_get_active(GTK_COMBO_BOX(dist_combo));
    
    // 计算A类不确定度
    double mean = calculate_mean(&meas_data);
    double std_dev = calculate_std_dev(&meas_data, mean);
    double u_a = calculate_type_a_uncertainty(&meas_data, std_dev);
    
    // 计算B类不确定度
    double u_b = calculate_type_b_uncertainty(&b_params);
    
    // 计算合成与扩展不确定度（k=2，95%置信水平）
    double u_c = calculate_combined_uncertainty(u_a, u_b);
    double expanded_uncert = u_c * 2;
    
    // 显示结果
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result_textview));
    gchar *result = g_strdup_printf(
        "测量结果统计（符合GB/T 27411-2012）\n"
        "----------------------------------------\n"
        "数据点数: %d\n"
        "平均值: %.6f\n"
        "实验标准差: %.6f\n"
        "\n【A类不确定度】\n"
        "平均值的标准不确定度: %.6f (u_A)\n"
        "\n【B类不确定度】\n"
        "极限误差: %.6f\n"
        "分布类型: %s\n"
        "标准不确定度: %.6f (u_B)\n"
        "\n【合成与扩展不确定度】\n"
        "合成标准不确定度: %.6f (u_c)\n"
        "扩展不确定度（k=2, 置信水平95%%）: %.6f\n"
        "测量结果: %.6f ± %.6f",
        meas_data.count, mean, std_dev,
        u_a,
        b_params.limit_error,
        (b_params.distribution_type == 0) ? "均匀分布 (k=√3)" : 
        (b_params.distribution_type == 1) ? "正态分布 (95%%, k=2)" : "正态分布 (99%%, k=3)",
        u_b,
        u_c, expanded_uncert, mean, expanded_uncert
    );
    gtk_text_buffer_set_text(buffer, result, -1);
    g_free(result);
}

// 清除所有数据和列表
void clear_data(GtkWidget *widget, gpointer user_data) {
    if (meas_data.data) {
        free(meas_data.data);
        meas_data.data = NULL;
        meas_data.count = 0;
    }
    // 清空列表
    gtk_list_store_clear(data_liststore);
    // 重置输入框
    gtk_entry_set_text(GTK_ENTRY(data_entry), "");
    gtk_entry_set_text(GTK_ENTRY(b_error_entry), "");
    gtk_combo_box_set_active(GTK_COMBO_BOX(dist_combo), 0);
    // 清空结果
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result_textview));
    gtk_text_buffer_set_text(buffer, "", -1);
}

// 创建数据列表视图
GtkWidget *create_data_listview() {
    // 创建列表存储模型（两列：序号、数据值）
    data_liststore = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_DOUBLE);
    
    // 创建树视图并关联模型
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data_liststore));
    g_object_unref(data_liststore);  // 减少引用计数，由树视图管理
    
    // 创建序号列
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "序号", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    // 创建数据值列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "测量值", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    return tree_view;
}

// 主窗口构建
void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *add_button;
    GtkWidget *calc_button;
    GtkWidget *clear_button;
    GtkWidget *scrolled_window;
    GtkWidget *data_listview;
    GtkWidget *frame;

    // 创建主窗口
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "不确定度计算器 (符合GB/T 27411-2012)");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // 创建网格布局
    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    // 数据输入区域
    label = gtk_label_new("输入测量数据:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    data_entry = gtk_entry_new();
    gtk_widget_set_hexpand(data_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), data_entry, 1, 0, 1, 1);

    add_button = gtk_button_new_with_label("添加数据");
    g_signal_connect(add_button, "clicked", G_CALLBACK(add_data_point), window);
    gtk_grid_attach(GTK_GRID(grid), add_button, 2, 0, 1, 1);

    // 已输入数据列表
    label = gtk_label_new("已输入数据:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 3, 1);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 2, 3, 4);  // 占据4行高度

    data_listview = create_data_listview();
    gtk_container_add(GTK_CONTAINER(scrolled_window), data_listview);

    // B类不确定度设置区域
    frame = gtk_frame_new("B类不确定度参数");
    gtk_grid_attach(GTK_GRID(grid), frame, 0, 6, 3, 2);
    gtk_widget_set_hexpand(frame, TRUE);

    GtkWidget *b_grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(frame), b_grid);
    gtk_grid_set_row_spacing(GTK_GRID(b_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(b_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(b_grid), 10);

    label = gtk_label_new("极限误差:");
    gtk_grid_attach(GTK_GRID(b_grid), label, 0, 0, 1, 1);

    b_error_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(b_grid), b_error_entry, 1, 0, 1, 1);

    label = gtk_label_new("分布类型:");
    gtk_grid_attach(GTK_GRID(b_grid), label, 2, 0, 1, 1);

    // 分布类型下拉菜单
    dist_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dist_combo), "均匀分布 (k=√3)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dist_combo), "正态分布 (95%, k=2)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dist_combo), "正态分布 (99%, k=3)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(dist_combo), 0);  // 默认均匀分布
    gtk_grid_attach(GTK_GRID(b_grid), dist_combo, 3, 0, 1, 1);

    // 操作按钮
    calc_button = gtk_button_new_with_label("计算不确定度");
    g_signal_connect(calc_button, "clicked", G_CALLBACK(calculate_uncertainty), window);
    gtk_grid_attach(GTK_GRID(grid), calc_button, 0, 8, 1, 1);

    clear_button = gtk_button_new_with_label("清除所有数据");
    g_signal_connect(clear_button, "clicked", G_CALLBACK(clear_data), window);
    gtk_grid_attach(GTK_GRID(grid), clear_button, 1, 8, 1, 1);

    // 结果显示区域
    label = gtk_label_new("计算结果:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 9, 3, 1);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 10, 3, 4);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);

    result_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(result_textview), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), result_textview);

    // 显示所有控件
    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.example.uncertaintycalculator", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    // 释放内存
    if (meas_data.data) {
        free(meas_data.data);
    }

    return status;
}
