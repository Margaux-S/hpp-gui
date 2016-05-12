#ifndef HPP_GUI_HPPMANIPULATIONWIDGETSPLUGIN_HH
#define HPP_GUI_HPPMANIPULATIONWIDGETSPLUGIN_HH

#include <QToolBar>

#include <gepetto/gui/plugin-interface.hh>
#include <hpp/corbaserver/manipulation/client.hh>
#undef __robot_hh__
#undef __problem_hh__
#include <hppwidgetsplugin/hppwidgetsplugin.hh>

namespace hpp {
  namespace gui {
    class HppManipulationWidgetsPlugin : public HppWidgetsPlugin
                                         // , public PluginInterface, public ModelInterface, public CorbaErrorInterface
    {
      Q_OBJECT
        Q_INTERFACES (gepetto::gui::PluginInterface
            gepetto::gui::ModelInterface
            gepetto::gui::CorbaInterface)

      public:
        typedef hpp::corbaServer::manipulation::Client HppManipClient;

        explicit HppManipulationWidgetsPlugin ();

        virtual ~HppManipulationWidgetsPlugin ();

        // PluginInterface interface
      public:
        void init();
        QString name() const;

        // ModelInterface interface
      public:
        void loadRobotModel (gepetto::gui::DialogLoadRobot::RobotDefinition rd);
        void loadEnvironmentModel (gepetto::gui::DialogLoadEnvironment::EnvironmentDefinition ed);
        std::string getBodyFromJoint (const std::string& jointName) const;
signals:
        void configurationValidationStatus (bool valid);

        // CorbaInterface
      public:
        virtual void openConnection ();
        virtual void closeConnection();

      public:
        HppManipClient* manipClient () const;

        void updateRobotJoints (const QString robotName);

        virtual Roadmap* createRoadmap (const std::string& jointName);

        public slots:
          void drawRobotContacts ();
          void drawEnvironmentContacts ();
          void drawHandlesFrame ();
          void drawGrippersFrame ();
          void autoBuildGraph();

        private slots:
          void buildGraph();

      private:
        // Type used to make one function to build datas needed for autoBuild
        typedef std::pair<hpp::Names_t, hpp::corbaserver::manipulation::Namess_t> NamesPair;
      typedef std::map<std::string, std::list<std::string> > MapNames;
        NamesPair buildNamess(const QList<QListWidgetItem *>& names);
        NamesPair convertMap(MapNames& mapNames);
        hpp::Names_t_var convertToNames(const QList<QListWidgetItem *>& l);
        
        HppManipClient* hpp_;

        QToolBar *toolBar_;
        QTabWidget *tw_;

        int firstEnter_;
    };
  } // namespace gui
} // namespace hpp

#endif // HPP_GUI_HPPMANIPULATIONWIDGETSPLUGIN_HH
