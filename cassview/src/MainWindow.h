#pragma once

#include "ui_MainWindow.h"
#include "CassandraModel.h"
#include "ContextModel.h"
#include "TableModel.h"
#include "RowModel.h"

#include <QtCassandra/QCassandra.h>
#include <QtGui>

class MainWindow
        : public QMainWindow
        , Ui::MainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onCellsModelReset();
    void on_action_Settings_triggered();
    void onAboutToQuit();
    void on_f_tables_currentIndexChanged(const QString &table_name);
    void on_f_contextCombo_currentIndexChanged(const QString &arg1);
    void onCurrentChanged( const QModelIndex & current, const QModelIndex & /*previous*/ );
    void on_action_About_triggered();
    void on_action_AboutQt_triggered();

private:
	typedef QtCassandra::QCassandra::pointer_t cassandra_t;
    cassandra_t  	f_cassandra;
    CassandraModel	f_cassandraModel;
    ContextModel	f_contextModel;
    TableModel		f_tableModel;
	RowModel		f_rowModel;
    QString			f_context;

    void		fillTableList();
    void        changeRow(const QModelIndex &index);
};

