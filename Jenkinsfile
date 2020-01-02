pipeline {
  agent any
  stages {
    stage('Hlavni_1') {
      parallel {
        stage('Prvni_msg') {
          steps {
            echo 'Prvni zprava'
          }
        }

        stage('wait 1') {
          steps {
            sleep 1
          }
        }

        stage('Wait 2') {
          steps {
            sleep 2
          }
        }

      }
    }

    stage('Hlavni_2') {
      parallel {
        stage('msg') {
          steps {
            echo 'Druha zprava'
          }
        }

        stage('wait 2') {
          steps {
            sleep 3
          }
        }

      }
    }

    stage('Hlavni_3; msg, wait') {
      steps {
        sleep 3
        echo 'Cekej 3 s'
      }
    }

    stage('Hlavni_4; msg, wait') {
      steps {
        sleep 2
        echo 'Cekej 2s a posledni zprava'
      }
    }

  }
}