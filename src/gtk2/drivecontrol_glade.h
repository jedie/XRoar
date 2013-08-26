static const gchar *drivecontrol_glade =
"<?xml version='1.0' encoding='UTF-8'?>"
"<interface>"
  "<requires lib='gtk+' version='2.16'/>"
  "<!-- interface-naming-policy project-wide -->"
  "<object class='GtkWindow' id='dc_window'>"
    "<property name='title' translatable='yes'>XRoar Drive Control</property>"
    "<child>"
      "<object class='GtkVBox' id='vbox1'>"
        "<property name='visible'>True</property>"
        "<child>"
          "<object class='GtkHBox' id='hbox1'>"
            "<property name='visible'>True</property>"
            "<property name='spacing'>12</property>"
            "<child>"
              "<object class='GtkLabel' id='label1'>"
                "<property name='visible'>True</property>"
                "<property name='label' translatable='yes'>Drive 1</property>"
                "<attributes>"
                  "<attribute name='weight' value='bold'/>"
                "</attributes>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkLabel' id='filename_drive1'>"
                "<property name='visible'>True</property>"
                "<property name='xalign'>0</property>"
                "<property name='ellipsize'>start</property>"
              "</object>"
              "<packing>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>0</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox2'>"
            "<property name='visible'>True</property>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox2'>"
                "<property name='visible'>True</property>"
                "<property name='homogeneous'>True</property>"
                "<property name='layout_style'>start</property>"
                "<child>"
                  "<object class='GtkCheckButton' id='we_drive1'>"
                    "<property name='label' translatable='yes'>Write enable</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkCheckButton' id='wb_drive1'>"
                    "<property name='label' translatable='yes'>Write back</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox1'>"
                "<property name='visible'>True</property>"
                "<child>"
                  "<object class='GtkButton' id='eject_drive1'>"
                    "<property name='label' translatable='yes'>Eject</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkButton' id='insert_drive1'>"
                    "<property name='label' translatable='yes'>Insert…</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>1</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHSeparator' id='hseparator1'>"
            "<property name='visible'>True</property>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>2</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox3'>"
            "<property name='visible'>True</property>"
            "<property name='spacing'>12</property>"
            "<child>"
              "<object class='GtkLabel' id='label2'>"
                "<property name='visible'>True</property>"
                "<property name='label' translatable='yes'>Drive 2</property>"
                "<attributes>"
                  "<attribute name='weight' value='bold'/>"
                "</attributes>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkLabel' id='filename_drive2'>"
                "<property name='visible'>True</property>"
                "<property name='xalign'>0</property>"
                "<property name='ellipsize'>start</property>"
              "</object>"
              "<packing>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>3</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox4'>"
            "<property name='visible'>True</property>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox3'>"
                "<property name='visible'>True</property>"
                "<property name='homogeneous'>True</property>"
                "<property name='layout_style'>start</property>"
                "<child>"
                  "<object class='GtkCheckButton' id='we_drive2'>"
                    "<property name='label' translatable='yes'>Write enable</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkCheckButton' id='wb_drive2'>"
                    "<property name='label' translatable='yes'>Write back</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox4'>"
                "<property name='visible'>True</property>"
                "<child>"
                  "<object class='GtkButton' id='eject_drive2'>"
                    "<property name='label' translatable='yes'>Eject</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkButton' id='insert_drive2'>"
                    "<property name='label' translatable='yes'>Insert…</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>4</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHSeparator' id='hseparator2'>"
            "<property name='visible'>True</property>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>5</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox5'>"
            "<property name='visible'>True</property>"
            "<child>"
              "<object class='GtkLabel' id='label3'>"
                "<property name='visible'>True</property>"
                "<property name='label' translatable='yes'>Drive 3</property>"
                "<attributes>"
                  "<attribute name='weight' value='bold'/>"
                "</attributes>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkLabel' id='filename_drive3'>"
                "<property name='visible'>True</property>"
                "<property name='xalign'>0</property>"
                "<property name='ellipsize'>start</property>"
              "</object>"
              "<packing>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>6</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox6'>"
            "<property name='visible'>True</property>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox5'>"
                "<property name='visible'>True</property>"
                "<property name='homogeneous'>True</property>"
                "<property name='layout_style'>start</property>"
                "<child>"
                  "<object class='GtkCheckButton' id='we_drive3'>"
                    "<property name='label' translatable='yes'>Write enable</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkCheckButton' id='wb_drive3'>"
                    "<property name='label' translatable='yes'>Write back</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox6'>"
                "<property name='visible'>True</property>"
                "<child>"
                  "<object class='GtkButton' id='eject_drive3'>"
                    "<property name='label' translatable='yes'>Eject</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkButton' id='insert_drive3'>"
                    "<property name='label' translatable='yes'>Insert…</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>7</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHSeparator' id='hseparator3'>"
            "<property name='visible'>True</property>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>8</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox7'>"
            "<property name='visible'>True</property>"
            "<property name='spacing'>12</property>"
            "<child>"
              "<object class='GtkLabel' id='label4'>"
                "<property name='visible'>True</property>"
                "<property name='label' translatable='yes'>Drive 4</property>"
                "<attributes>"
                  "<attribute name='weight' value='bold'/>"
                "</attributes>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkLabel' id='filename_drive4'>"
                "<property name='visible'>True</property>"
                "<property name='xalign'>0</property>"
                "<property name='ellipsize'>start</property>"
              "</object>"
              "<packing>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>9</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHBox' id='hbox8'>"
            "<property name='visible'>True</property>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox7'>"
                "<property name='visible'>True</property>"
                "<property name='homogeneous'>True</property>"
                "<property name='layout_style'>start</property>"
                "<child>"
                  "<object class='GtkCheckButton' id='we_drive4'>"
                    "<property name='label' translatable='yes'>Write enable</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkCheckButton' id='wb_drive4'>"
                    "<property name='label' translatable='yes'>Write back</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>False</property>"
                    "<property name='draw_indicator'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='position'>0</property>"
              "</packing>"
            "</child>"
            "<child>"
              "<object class='GtkHButtonBox' id='hbuttonbox8'>"
                "<property name='visible'>True</property>"
                "<child>"
                  "<object class='GtkButton' id='eject_drive4'>"
                    "<property name='label' translatable='yes'>Eject</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>0</property>"
                  "</packing>"
                "</child>"
                "<child>"
                  "<object class='GtkButton' id='insert_drive4'>"
                    "<property name='label' translatable='yes'>Insert…</property>"
                    "<property name='visible'>True</property>"
                    "<property name='can_focus'>True</property>"
                    "<property name='receives_default'>True</property>"
                  "</object>"
                  "<packing>"
                    "<property name='expand'>False</property>"
                    "<property name='fill'>False</property>"
                    "<property name='position'>1</property>"
                  "</packing>"
                "</child>"
              "</object>"
              "<packing>"
                "<property name='expand'>False</property>"
                "<property name='position'>1</property>"
              "</packing>"
            "</child>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>10</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkHSeparator' id='hseparator4'>"
            "<property name='visible'>True</property>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>11</property>"
          "</packing>"
        "</child>"
        "<child>"
          "<object class='GtkLabel' id='drive_cyl_head'>"
            "<property name='visible'>True</property>"
            "<property name='label' translatable='yes'>Dr ? Tr ?? He ?</property>"
          "</object>"
          "<packing>"
            "<property name='expand'>False</property>"
            "<property name='position'>12</property>"
          "</packing>"
        "</child>"
      "</object>"
    "</child>"
  "</object>"
"</interface>"
;
