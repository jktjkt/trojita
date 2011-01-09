#ifndef FAKECAPABILITIESINJECTOR_H
#define FAKECAPABILITIESINJECTOR_H

#include "Imap/Model/Model.h"

/** @short Class for persuading the model that the parser supports certain capabilities */
class FakeCapabilitiesInjector
{
public:
    FakeCapabilitiesInjector(Imap::Mailbox::Model *_model): model(_model)
    {}

    /** @short Add the specified capability to the list of capabilities "supported" by the server */
    void injectCapability(const QString& cap)
    {
        Q_ASSERT(!model->_parsers.isEmpty());
        model->updateCapabilities( model->_parsers.begin().key(), model->capabilities() << cap );
    }
private:
    Imap::Mailbox::Model *model;
};

#endif // FAKECAPABILITIESINJECTOR_H
